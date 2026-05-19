// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// GPL v3.0 — BoredOS mlibc sysdep backend (sysdeps.cpp)
//
// Maps mlibc's abstract sysdep API to BoredOS kernel syscalls.
// BoredOS uses int 0x80 with the following register convention:
//   rax = syscall number
//   rdi = arg1, rsi = arg2, rdx = arg3, r10 = arg4, r8 = arg5, r9 = arg6
// Return value in rax.

#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include <abi-bits/seek-whence.h>
#include <abi-bits/vm-flags.h>
#include <termios.h>
#include <stdarg.h>
#include <sys/select.h>
#include <poll.h>

// ---------------------------------------------------------------------------
// BoredOS syscall numbers (must match src/sys/syscall.h)
// ---------------------------------------------------------------------------
#define BORED_SYS_EXIT    0
#define BORED_SYS_WRITE   1
#define BORED_SYS_FS      4
#define BORED_SYS_SYSTEM  5
#define BORED_SYS_SBRK    9
#define BORED_SYS_MMAP   11
#define BORED_SYS_MUNMAP 12
#define BORED_SYS_FUTEX  13

// FS sub-commands
#define FS_CMD_OPEN   1
#define FS_CMD_READ   2
#define FS_CMD_WRITE  3
#define FS_CMD_CLOSE  4
#define FS_CMD_SEEK   5
#define FS_CMD_TELL   6
#define FS_CMD_SIZE   9
#define FS_CMD_MKDIR 10
#define FS_CMD_EXISTS 11
#define FS_CMD_GETCWD 12
#define FS_CMD_CHDIR  13
#define FS_CMD_DELETE  8
#define FS_CMD_DUP    15
#define FS_CMD_DUP2   16
#define FS_CMD_PIPE   17
#define FS_CMD_FCNTL  18
#define FS_CMD_IOCTL  24
#define FS_CMD_POLL   22

// System sub-commands
#define SYSTEM_CMD_RTC_GET    11
#define SYSTEM_CMD_REBOOT     12
#define SYSTEM_CMD_SHUTDOWN   13
#define SYSTEM_CMD_GET_TICKS  16
#define SYSTEM_CMD_SLEEP      46
#define SYSTEM_CMD_SPAWN      64
#define SYSTEM_CMD_EXEC       70
#define SYSTEM_CMD_WAITPID    71
#define SYSTEM_CMD_KILL_SIGNAL 72
#define SYSTEM_CMD_FORK       80

// Futex operations
#define BORED_FUTEX_WAIT 0
#define BORED_FUTEX_WAKE 1

// ---------------------------------------------------------------------------
// Raw syscall helpers (inline asm — int 0x80 ABI)
// ---------------------------------------------------------------------------
static inline uint64_t _sc0(uint64_t n) {
    uint64_t r;
    asm volatile("int $0x80" : "=a"(r) : "a"(n) : "rcx","r11","memory");
    return r;
}
static inline uint64_t _sc1(uint64_t n, uint64_t a1) {
    uint64_t r;
    asm volatile("int $0x80" : "=a"(r) : "a"(n),"D"(a1) : "rcx","r11","memory");
    return r;
}
static inline uint64_t _sc2(uint64_t n, uint64_t a1, uint64_t a2) {
    uint64_t r;
    asm volatile("int $0x80" : "=a"(r) : "a"(n),"D"(a1),"S"(a2) : "rcx","r11","memory");
    return r;
}
static inline uint64_t _sc3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t r;
    asm volatile("int $0x80" : "=a"(r) : "a"(n),"D"(a1),"S"(a2),"d"(a3)
                 : "rcx","r11","r10","r8","r9","memory");
    return r;
}
static inline uint64_t _sc4(uint64_t n, uint64_t a1, uint64_t a2,
                             uint64_t a3, uint64_t a4) {
    uint64_t r;
    register uint64_t r10 asm("r10") = a4;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(n),"D"(a1),"S"(a2),"d"(a3),"r"(r10)
                 : "rcx","r11","r8","r9","memory");
    return r;
}
static inline uint64_t _sc5(uint64_t n, uint64_t a1, uint64_t a2,
                             uint64_t a3, uint64_t a4, uint64_t a5) {
    uint64_t r;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(n),"D"(a1),"S"(a2),"d"(a3),"r"(r10),"r"(r8)
                 : "rcx","r11","r9","memory");
    return r;
}
static inline uint64_t _sc6(uint64_t n, uint64_t a1, uint64_t a2,
                             uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    uint64_t r;
    register uint64_t r10 asm("r10") = a4;
    register uint64_t r8  asm("r8")  = a5;
    register uint64_t r9  asm("r9")  = a6;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(n),"D"(a1),"S"(a2),"d"(a3),"r"(r10),"r"(r8),"r"(r9)
                 : "rcx","r11","memory");
    return r;
}

// ---------------------------------------------------------------------------
// RTC struct (matches kernel SYSTEM_CMD_RTC_GET response)
// ---------------------------------------------------------------------------
struct bored_rtc {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    uint8_t  _pad;
};

// ---------------------------------------------------------------------------
// mlibc namespace: mandatory sysdeps implemented via SysdepImpl specializations
// ---------------------------------------------------------------------------
namespace mlibc {

static struct termios g_tty_termios = []() {
    struct termios t;
    __builtin_memset(&t, 0, sizeof(t));
    t.c_iflag = ICRNL | IXON;
    t.c_oflag = OPOST | ONLCR;
    t.c_cflag = CS8 | CREAD;
    t.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    t.c_cc[VINTR] = 3;   // Ctrl+C
    t.c_cc[VQUIT] = 28;  // Ctrl+Backslash
    t.c_cc[VEOF] = 4;    // Ctrl+D
    t.c_cc[VSTART] = 17; // Ctrl+Q
    t.c_cc[VSTOP] = 19;  // Ctrl+S
    t.c_cc[VSUSP] = 26;  // Ctrl+Z
    return t;
}();

static char g_canon_buffer[2048];
static size_t g_canon_head = 0;
static size_t g_canon_tail = 0;

extern "C" void *__dso_handle = nullptr;

struct boredos_fat_file_info_t {
    char name[256];
    uint32_t size;
    uint8_t is_directory;
    uint32_t start_cluster;
    uint16_t write_date;
    uint16_t write_time;
};

struct BoredDirHandle {
    char path[256];
    boredos_fat_file_info_t *entries;
    int entry_count;
    int current_index;
    bool active;
};

static BoredDirHandle open_dirs[16];

// --- Exit ---
void SysdepImpl<Exit>::operator()(int status) {
    _sc1(BORED_SYS_EXIT, (uint64_t)(unsigned)status);
    __builtin_unreachable();
}

// --- Futex ---
int SysdepImpl<FutexWait>::operator()(int *pointer, int expected, const struct timespec *) {
    int rc = (int)_sc3(BORED_SYS_FUTEX,
                       (uint64_t)(uintptr_t)pointer,
                       (uint64_t)BORED_FUTEX_WAIT,
                       (uint64_t)(uint32_t)expected);
    if (rc == 0) return 0;
    if (rc == -11) return EAGAIN;
    return EINTR;
}

int SysdepImpl<FutexWake>::operator()(int *pointer, bool all) {
    _sc3(BORED_SYS_FUTEX,
         (uint64_t)(uintptr_t)pointer,
         (uint64_t)BORED_FUTEX_WAKE,
         all ? (uint64_t)INT32_MAX : 1ULL);
    return 0;
}

// --- Open ---
int SysdepImpl<Open>::operator()(const char *path, int flags, mode_t mode, int *fd) {
    (void)mode;
    const char *mstr = "r";
    if ((flags & O_ACCMODE) == O_WRONLY) mstr = "w";
    else if ((flags & O_ACCMODE) == O_RDWR) mstr = "rw";
    if (flags & O_APPEND) mstr = "a";

    int ret = (int)_sc3(BORED_SYS_FS,
                        (uint64_t)FS_CMD_OPEN,
                        (uint64_t)(uintptr_t)path,
                        (uint64_t)(uintptr_t)mstr);
    if (ret < 0) return ENOENT;
    *fd = ret;
    return 0;
}

// --- Read ---
int SysdepImpl<Read>::operator()(int fd, void *buf, size_t count, ssize_t *bytes_read) {
    if (count == 0) {
        *bytes_read = 0;
        return 0;
    }

    int fflags = 0;
    int f_ret = (int)_sc4(BORED_SYS_FS, (uint64_t)FS_CMD_FCNTL, (uint64_t)fd, (uint64_t)3 /* F_GETFL */, 0);
    if (f_ret >= 0) fflags = f_ret;

    // Check if it's a TTY
    if (SysdepImpl<Isatty>::operator()(fd) == 0) {
        if (g_tty_termios.c_lflag & ICANON) {
            // Canonical (line-buffered) mode
            char *out_buf = (char *)buf;
            
            // If the buffer is empty, read a new line
            if (g_canon_head == g_canon_tail) {
                g_canon_head = 0;
                g_canon_tail = 0;
                
                while (true) {
                    // If not non-blocking, block until character is available
                    if (!(fflags & 0x0800 /* O_NONBLOCK */)) {
                        struct bored_pollfd {
                            int fd;
                            short events;
                            short revents;
                        } pfd;
                        pfd.fd = fd;
                        pfd.events = 0x0001; // POLLIN
                        pfd.revents = 0;
                        int rc;
                        while ((rc = (int)_sc4(BORED_SYS_FS, (uint64_t)FS_CMD_POLL, (uint64_t)&pfd, 1ULL, (uint64_t)-1)) == -2);
                    }
                    
                    char ch = 0;
                    int read_ret = (int)_sc4(BORED_SYS_FS,
                                             (uint64_t)FS_CMD_READ,
                                             (uint64_t)fd,
                                             (uint64_t)(uintptr_t)&ch,
                                             1ULL);
                    if (read_ret < 0) {
                        return EIO;
                    }
                    if (read_ret == 0) {
                        if (fflags & 0x0800 /* O_NONBLOCK */) {
                            // Non-blocking EOF / EAGAIN
                            if (g_canon_tail == 0) {
                                *bytes_read = 0;
                                return 0;
                            }
                        }
                        break; // EOF
                    }
                    
                    // Handle backspace / erase
                    if (ch == '\b' || ch == 127) {
                        if (g_canon_tail > 0) {
                            g_canon_tail--;
                            if (g_tty_termios.c_lflag & ECHO) {
                                // Erase the character visually from screen
                                const char erase_seq[] = "\b \b";
                                _sc4(BORED_SYS_FS, (uint64_t)FS_CMD_WRITE, 1ULL, (uint64_t)(uintptr_t)erase_seq, 3ULL);
                            }
                        }
                        continue;
                    }
                    
                    // Handle newlines
                    if (ch == '\r' || ch == '\n') {
                        char newline_char = '\n';
                        if (ch == '\r' && !(g_tty_termios.c_iflag & ICRNL)) {
                            newline_char = '\r';
                        }
                        
                        if (g_canon_tail < sizeof(g_canon_buffer)) {
                            g_canon_buffer[g_canon_tail++] = newline_char;
                        }
                        
                        if (g_tty_termios.c_lflag & ECHO) {
                            _sc4(BORED_SYS_FS, (uint64_t)FS_CMD_WRITE, 1ULL, (uint64_t)(uintptr_t)&newline_char, 1ULL);
                        }
                        break;
                    }
                    
                    // Store regular character
                    if (g_canon_tail < sizeof(g_canon_buffer)) {
                        g_canon_buffer[g_canon_tail++] = ch;
                    }
                    
                    if (g_tty_termios.c_lflag & ECHO) {
                        _sc4(BORED_SYS_FS, (uint64_t)FS_CMD_WRITE, 1ULL, (uint64_t)(uintptr_t)&ch, 1ULL);
                    }
                }
            }
            
            // Copy from canon buffer to user
            size_t copied = 0;
            while (copied < count && g_canon_head < g_canon_tail) {
                char ch = g_canon_buffer[g_canon_head++];
                out_buf[copied++] = ch;
                if (ch == '\n') {
                    break;
                }
            }
            
            if (g_canon_head == g_canon_tail) {
                g_canon_head = 0;
                g_canon_tail = 0;
            }
            
            *bytes_read = copied;
            return 0;
        } else {
            // Non-canonical mode
            if (!(fflags & 0x0800 /* O_NONBLOCK */)) {
                struct bored_pollfd {
                    int fd;
                    short events;
                    short revents;
                } pfd;
                pfd.fd = fd;
                pfd.events = 0x0001; // POLLIN
                pfd.revents = 0;
                int rc;
                while ((rc = (int)_sc4(BORED_SYS_FS, (uint64_t)FS_CMD_POLL, (uint64_t)&pfd, 1ULL, (uint64_t)-1)) == -2);
            }

            int ret = (int)_sc4(BORED_SYS_FS,
                                (uint64_t)FS_CMD_READ,
                                (uint64_t)fd,
                                (uint64_t)(uintptr_t)buf,
                                (uint64_t)count);
            if (ret < 0) return EIO;
            
            if (ret > 0 && (g_tty_termios.c_lflag & ECHO)) {
                _sc4(BORED_SYS_FS, (uint64_t)FS_CMD_WRITE, 1ULL, (uint64_t)(uintptr_t)buf, (uint64_t)ret);
            }
            
            *bytes_read = ret;
            return 0;
        }
    }

    // Default non-terminal file read
    if (!(fflags & 0x0800 /* O_NONBLOCK */)) {
        struct bored_pollfd {
            int fd;
            short events;
            short revents;
        } pfd;
        pfd.fd = fd;
        pfd.events = 0x0001; // POLLIN
        pfd.revents = 0;
        int rc;
        while ((rc = (int)_sc4(BORED_SYS_FS, (uint64_t)FS_CMD_POLL, (uint64_t)&pfd, 1ULL, (uint64_t)-1)) == -2);
    }

    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_READ,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)buf,
                        (uint64_t)count);
    if (ret < 0) return EIO;
    *bytes_read = ret;
    return 0;
}

// --- Write ---
int SysdepImpl<Write>::operator()(int fd, const void *buf, size_t count, ssize_t *bytes_written) {
    int ret;
    if (fd == 1 || fd == 2) {
        ret = (int)_sc3(BORED_SYS_WRITE,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)buf,
                        (uint64_t)count);
    } else {
        ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_WRITE,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)buf,
                        (uint64_t)count);
    }
    if (ret < 0) return EIO;
    *bytes_written = ret;
    return 0;
}

// --- Seek ---
int SysdepImpl<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_SEEK,
                        (uint64_t)fd,
                        (uint64_t)(int64_t)offset,
                        (uint64_t)whence);
    if (ret < 0) return ESPIPE;
    *new_offset = ret;
    return 0;
}

// --- Close ---
int SysdepImpl<Close>::operator()(int fd) {
    if (fd >= 1000 && fd < 1016) {
        int slot = fd - 1000;
        if (open_dirs[slot].active) {
            if (open_dirs[slot].entries) {
                __builtin_free(open_dirs[slot].entries);
                open_dirs[slot].entries = nullptr;
            }
            open_dirs[slot].active = false;
        }
        return 0;
    }
    _sc2(BORED_SYS_FS, (uint64_t)FS_CMD_CLOSE, (uint64_t)fd);
    return 0;
}

// --- Clock/time ---
int SysdepImpl<ClockGet>::operator()(int clock, time_t *secs, long *nanos) {
    (void)clock;
    struct bored_rtc rtc = {};
    _sc2(BORED_SYS_SYSTEM,
         (uint64_t)SYSTEM_CMD_RTC_GET,
         (uint64_t)(uintptr_t)&rtc);

    uint64_t days = 0;
    for (int y = 2000; y < (int)rtc.year; y++) {
        days += ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;
    }
    static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int m = 1; m < (int)rtc.month; m++) {
        days += mdays[m - 1];
        if (m == 2 && ((rtc.year % 4 == 0 && rtc.year % 100 != 0) || rtc.year % 400 == 0))
            days++;
    }
    days += rtc.day - 1;

    uint64_t s = days * 86400ULL
               + (uint64_t)rtc.hour   * 3600
               + (uint64_t)rtc.minute * 60
               + rtc.second;

    s += 946684800ULL;

    *secs = (time_t)s;
    *nanos = 0;
    return 0;
}

// --- Sleep ---
int SysdepImpl<Sleep>::operator()(time_t *secs, long *nanos) {
    uint64_t ms = (*secs) * 1000 + (*nanos) / 1000000;
    _sc2(BORED_SYS_SYSTEM,
         (uint64_t)SYSTEM_CMD_SLEEP,
         (uint64_t)ms);
    *secs = 0;
    *nanos = 0;
    return 0;
}

// --- LibcLog ---
void SysdepImpl<LibcLog>::operator()(const char *msg) {
    if (!msg) return;
    size_t len = 0;
    while (msg[len]) len++;
    _sc3(BORED_SYS_WRITE,
         (uint64_t)2,
         (uint64_t)(uintptr_t)msg,
         (uint64_t)len);
}

// --- LibcPanic ---
[[noreturn]] void SysdepImpl<LibcPanic>::operator()() {
    const char *msg = "mlibc panic!\n";
    size_t len = 13;
    _sc3(BORED_SYS_WRITE, 2ULL, (uint64_t)(uintptr_t)msg, (uint64_t)len);
    _sc1(BORED_SYS_EXIT, (uint64_t)(-1ULL));
    __builtin_unreachable();
}

// --- Anonymous memory allocation (heap) ---
int SysdepImpl<AnonAllocate>::operator()(size_t size, void **pointer) {
    void *ptr = (void *)_sc6(BORED_SYS_MMAP,
                             0,
                             (uint64_t)size,
                             3,
                             0x22,
                             (uint64_t)(uint32_t)-1,
                             0);
    if (!ptr || (uintptr_t)ptr >= (uintptr_t)-4096ULL)
        return ENOMEM;
    *pointer = ptr;
    return 0;
}

int SysdepImpl<AnonFree>::operator()(void *pointer, size_t size) {
    _sc2(BORED_SYS_MUNMAP, (uint64_t)(uintptr_t)pointer, (uint64_t)size);
    return 0;
}

// --- VM map/unmap ---
int SysdepImpl<VmMap>::operator()(void *hint, size_t size, int prot, int flags,
                                  int fd, off_t offset, void **window) {
    void *ptr = (void *)_sc6(BORED_SYS_MMAP,
                             (uint64_t)(uintptr_t)hint,
                             (uint64_t)size,
                             (uint64_t)(uint32_t)prot,
                             (uint64_t)(uint32_t)flags,
                             (uint64_t)(uint32_t)fd,
                             (uint64_t)(int64_t)offset);
    if (!ptr || (uintptr_t)ptr >= (uintptr_t)-4096ULL)
        return ENOMEM;
    *window = ptr;
    return 0;
}

int SysdepImpl<VmUnmap>::operator()(void *pointer, size_t size) {
    _sc2(BORED_SYS_MUNMAP, (uint64_t)(uintptr_t)pointer, (uint64_t)size);
    return 0;
}

// --- TCB set ---
int SysdepImpl<TcbSet>::operator()(void *pointer) {
    _sc2(5, 79, (uint64_t)(uintptr_t)pointer); // 5 = BORED_SYS_SYSTEM, 79 = SYSTEM_CMD_SET_FS_BASE
    return 0;
}

// --- dup / dup2 ---
int SysdepImpl<Dup>::operator()(int fd, int flags, int *new_fd) {
    (void)flags;
    int ret = (int)_sc2(BORED_SYS_FS, (uint64_t)FS_CMD_DUP, (uint64_t)fd);
    if (ret < 0) return EBADF;
    *new_fd = ret;
    return 0;
}

int SysdepImpl<Dup2>::operator()(int fd, int flags, int new_fd) {
    (void)flags;
    int ret = (int)_sc3(BORED_SYS_FS,
                        (uint64_t)FS_CMD_DUP2,
                        (uint64_t)fd,
                        (uint64_t)new_fd);
    if (ret < 0) return EBADF;
    return 0;
}

// --- pipe ---
int SysdepImpl<Pipe>::operator()(int *fds, int flags) {
    (void)flags;
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_PIPE,
                        (uint64_t)(uintptr_t)fds);
    return ret < 0 ? EMFILE : 0;
}

// --- fcntl ---
int SysdepImpl<Fcntl>::operator()(int fd, int request, va_list args, int *result) {
    int val = va_arg(args, int);
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_FCNTL,
                        (uint64_t)fd,
                        (uint64_t)request,
                        (uint64_t)val);
    if (ret < 0) return EBADF;
    *result = ret;
    return 0;
}

// --- ioctl ---
int SysdepImpl<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result) {
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_IOCTL,
                        (uint64_t)fd,
                        (uint64_t)request,
                        (uint64_t)(uintptr_t)arg);
    if (ret < 0) return EINVAL;
    if (result) *result = ret;
    return 0;
}



// --- stat / fstat ---
int SysdepImpl<Stat>::operator()(mlibc::fsfd_target fsfdt, int fd, const char *path,
                                 int flags, struct stat *result) {
    (void)flags;
    if (!result) return EFAULT;
    __builtin_memset(result, 0, sizeof(*result));

    if (fsfdt == mlibc::fsfd_target::path) {
        boredos_fat_file_info_t info = {};
        int ret = (int)_sc3(BORED_SYS_FS,
                            (uint64_t)14, // FS_CMD_GET_INFO
                            (uint64_t)(uintptr_t)path,
                            (uint64_t)(uintptr_t)&info);
        if (ret < 0) return ENOENT;
        
        result->st_size = info.size;
        if (info.is_directory) {
            result->st_mode = 0040755; // S_IFDIR | 0755
        } else {
            result->st_mode = 0100644; // S_IFREG | 0644
        }
    } else {
        // fd target
        if (fd >= 0 && fd <= 2) {
            result->st_mode = 0020666; // S_IFCHR | 0666
            result->st_size = 0;
        } else {
            int size = (int)_sc2(BORED_SYS_FS, (uint64_t)9, (uint64_t)fd); // FS_CMD_SIZE
            result->st_size = size >= 0 ? size : 0;
            result->st_mode = 0100644; // S_IFREG | 0644
        }
    }
    return 0;
}

// --- access ---
int SysdepImpl<Access>::operator()(const char *path, int mode) {
    (void)mode;
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_EXISTS,
                        (uint64_t)(uintptr_t)path);
    return ret == 1 ? 0 : ENOENT;
}

// --- mkdir / unlink / rmdir ---
int SysdepImpl<Mkdir>::operator()(const char *path, mode_t mode) {
    (void)mode;
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_MKDIR,
                        (uint64_t)(uintptr_t)path);
    return ret < 0 ? EACCES : 0;
}

int SysdepImpl<Unlinkat>::operator()(int dirfd, const char *path, int flags) {
    (void)dirfd; (void)flags;
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_DELETE,
                        (uint64_t)(uintptr_t)path);
    return ret < 0 ? ENOENT : 0;
}

int SysdepImpl<Rmdir>::operator()(const char *path) {
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_DELETE,
                        (uint64_t)(uintptr_t)path);
    return ret < 0 ? ENOENT : 0;
}

// --- getcwd / chdir ---
int SysdepImpl<GetCwd>::operator()(char *buffer, size_t size) {
    int ret = (int)_sc3(BORED_SYS_FS,
                        (uint64_t)FS_CMD_GETCWD,
                        (uint64_t)(uintptr_t)buffer,
                        (uint64_t)size);
    return ret < 0 ? ERANGE : 0;
}

int SysdepImpl<Chdir>::operator()(const char *path) {
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_CHDIR,
                        (uint64_t)(uintptr_t)path);
    return ret < 0 ? ENOENT : 0;
}

// --- fork ---
int SysdepImpl<Fork>::operator()(pid_t *child) {
    int ret = (int)_sc2(BORED_SYS_SYSTEM,
                        (uint64_t)SYSTEM_CMD_FORK,
                        0ULL);
    if (ret < 0) return -ret;
    *child = ret;
    return 0;
}

// --- execve ---
int SysdepImpl<Execve>::operator()(const char *path, char *const argv[], char *const envp[]) {
    (void)envp;
    static char argbuf[512];
    argbuf[0] = '\0';
    if (argv && argv[1]) {
        size_t pos = 0;
        for (int i = 1; argv[i] && pos < sizeof(argbuf) - 2; i++) {
            if (i > 1) argbuf[pos++] = ' ';
            const char *a = argv[i];
            while (*a && pos < sizeof(argbuf) - 1) argbuf[pos++] = *a++;
        }
        argbuf[pos] = '\0';
    }
    _sc4(BORED_SYS_SYSTEM,
         (uint64_t)SYSTEM_CMD_EXEC,
         (uint64_t)(uintptr_t)path,
         (uint64_t)(uintptr_t)argbuf,
         0ULL);
    return ENOEXEC;
}

// --- waitpid ---
int SysdepImpl<Waitpid>::operator()(pid_t pid, int *status, int flags, struct rusage *ru, pid_t *ret_pid) {
    (void)ru;
    int ret = (int)_sc4(BORED_SYS_SYSTEM,
                        (uint64_t)SYSTEM_CMD_WAITPID,
                        (uint64_t)(int64_t)pid,
                        (uint64_t)(uintptr_t)status,
                        (uint64_t)flags);
    if (ret < 0) return ECHILD;
    *ret_pid = (pid_t)ret;
    return 0;
}

// --- kill ---
int SysdepImpl<Kill>::operator()(pid_t pid, int sig) {
    int ret = (int)_sc4(BORED_SYS_SYSTEM,
                        (uint64_t)SYSTEM_CMD_KILL_SIGNAL,
                        (uint64_t)(int64_t)pid,
                        (uint64_t)sig,
                        0ULL);
    return ret < 0 ? ESRCH : 0;
}

// --- signal ---
int SysdepImpl<Sigaction>::operator()(int signum, const struct sigaction *act, struct sigaction *oldact) {
    (void)signum; (void)act; (void)oldact;
    return 0;
}

int SysdepImpl<Sigprocmask>::operator()(int how, const sigset_t *set, sigset_t *retrieve) {
    (void)how; (void)set; (void)retrieve;
    return 0;
}

// --- isatty ---
int SysdepImpl<Isatty>::operator()(int fd) {
    return (fd >= 0 && fd <= 2) ? 0 : ENOTTY;
}

// --- opendir / readentries ---
int SysdepImpl<OpenDir>::operator()(const char *path, int *handle) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < 16; i++) {
        if (!open_dirs[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return EMFILE;

    // Read entries using BORED_SYS_FS subcommand FS_CMD_LIST (7)
    boredos_fat_file_info_t *ents = (boredos_fat_file_info_t *)__builtin_malloc(sizeof(boredos_fat_file_info_t) * 256);
    if (!ents) return ENOMEM;

    int count = (int)_sc4(BORED_SYS_FS,
                         (uint64_t)7, // FS_CMD_LIST
                         (uint64_t)(uintptr_t)path,
                         (uint64_t)(uintptr_t)ents,
                         256ULL);
    if (count < 0) {
        __builtin_free(ents);
        return ENOENT;
    }

    open_dirs[slot].entries = ents;
    open_dirs[slot].entry_count = count;
    open_dirs[slot].current_index = 0;
    __builtin_strncpy(open_dirs[slot].path, path, 255);
    open_dirs[slot].active = true;

    *handle = slot + 1000; // Offset to avoid overlapping with standard fds
    return 0;
}

int SysdepImpl<ReadEntries>::operator()(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
    int slot = handle - 1000;
    if (slot < 0 || slot >= 16 || !open_dirs[slot].active) return EBADF;

    BoredDirHandle &dir = open_dirs[slot];
    if (dir.current_index >= dir.entry_count) {
        *bytes_read = 0;
        return 0;
    }

    // Format the entry into the buffer as a struct dirent
    struct mlibc_dirent {
        uint64_t d_ino;
        int64_t d_off;
        uint16_t d_reclen;
        uint8_t d_type;
        char d_name[256];
    };

    if (max_size < sizeof(mlibc_dirent)) {
        return EINVAL;
    }

    mlibc_dirent *out = (mlibc_dirent *)buffer;
    __builtin_memset(out, 0, sizeof(mlibc_dirent));

    boredos_fat_file_info_t &in = dir.entries[dir.current_index];
    
    out->d_ino = dir.current_index + 1;
    out->d_off = dir.current_index + 1;
    out->d_reclen = sizeof(mlibc_dirent);
    out->d_type = in.is_directory ? 4 : 8; // DT_DIR is 4, DT_REG is 8
    __builtin_strncpy(out->d_name, in.name, 255);

    dir.current_index++;
    *bytes_read = sizeof(mlibc_dirent);
    return 0;
}

// --- socket ---
#define FS_CMD_UNIX_SOCKET_CREATE 25
#define FS_CMD_UNIX_SOCKET_BIND 26
#define FS_CMD_UNIX_SOCKET_LISTEN 27
#define FS_CMD_UNIX_SOCKET_ACCEPT 28
#define FS_CMD_UNIX_SOCKET_CONNECT 29

int SysdepImpl<Socket>::operator()(int family, int type, int protocol, int *fd) {
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_UNIX_SOCKET_CREATE,
                        (uint64_t)family,
                        (uint64_t)type,
                        (uint64_t)protocol);
    if (ret < 0) return EACCES;
    *fd = ret;
    return 0;
}

int SysdepImpl<Bind>::operator()(int fd, const struct sockaddr *addr_ptr, socklen_t addr_length) {
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_UNIX_SOCKET_BIND,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)addr_ptr,
                        (uint64_t)addr_length);
    return ret < 0 ? EACCES : 0;
}

int SysdepImpl<Listen>::operator()(int fd, int backlog) {
    (void)backlog;
    int ret = (int)_sc2(BORED_SYS_FS,
                        (uint64_t)FS_CMD_UNIX_SOCKET_LISTEN,
                        (uint64_t)fd);
    return ret < 0 ? EACCES : 0;
}

int SysdepImpl<Connect>::operator()(int fd, const struct sockaddr *addr_ptr, socklen_t addr_length) {
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_UNIX_SOCKET_CONNECT,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)addr_ptr,
                        (uint64_t)addr_length);
    return ret < 0 ? ECONNREFUSED : 0;
}

int SysdepImpl<Accept>::operator()(int fd, int *newfd, struct sockaddr *addr_ptr, socklen_t *addr_length, int flags) {
    (void)flags;
    int ret = (int)_sc4(BORED_SYS_FS,
                        (uint64_t)FS_CMD_UNIX_SOCKET_ACCEPT,
                        (uint64_t)fd,
                        (uint64_t)(uintptr_t)addr_ptr,
                        (uint64_t)(uintptr_t)addr_length);
    if (ret < 0) return ret == -2 ? EAGAIN : EACCES;
    *newfd = ret;
    return 0;
}

#ifndef TIOCGWINSZ
#define TIOCGWINSZ 0x5413
#endif

// --- tcgetattr ---
int SysdepImpl<Tcgetattr>::operator()(int fd, struct termios *attr) {
    if (SysdepImpl<Isatty>::operator()(fd) != 0) {
        return ENOTTY;
    }

    __builtin_memcpy(attr, &g_tty_termios, sizeof(*attr));
    return 0;
}

// --- tcsetattr ---
int SysdepImpl<Tcsetattr>::operator()(int fd, int optional_actions, const struct termios *attr) {
    (void)optional_actions;
    if (SysdepImpl<Isatty>::operator()(fd) != 0) {
        return ENOTTY;
    }
    __builtin_memcpy(&g_tty_termios, attr, sizeof(*attr));
    return 0;
}

// --- tcgetwinsize ---
int SysdepImpl<Tcgetwinsize>::operator()(int fd, struct winsize *winsz) {
    int result = 0;
    int err = SysdepImpl<Ioctl>::operator()(fd, TIOCGWINSZ, winsz, &result);
    return err;
}

// --- tcsetwinsize ---
int SysdepImpl<Tcsetwinsize>::operator()(int fd, const struct winsize *winsz) {
    (void)winsz;
    if (SysdepImpl<Isatty>::operator()(fd) != 0) {
        return ENOTTY;
    }
    return 0;
}

// --- pselect ---
int SysdepImpl<Pselect>::operator()(int num_fds, fd_set *read_set, fd_set *write_set, fd_set *except_set, const struct timespec *timeout, const sigset_t *sigmask, int *num_events) {
    (void)sigmask;
    int count = 0;
    for (int fd = 0; fd < num_fds; fd++) {
        bool r = read_set && FD_ISSET(fd, read_set);
        bool w = write_set && FD_ISSET(fd, write_set);
        bool e = except_set && FD_ISSET(fd, except_set);
        if (r || w || e) count++;
    }

    struct pollfd *pfds = nullptr;
    if (count > 0) {
        pfds = (struct pollfd *)__builtin_alloca(sizeof(struct pollfd) * count);
        int idx = 0;
        for (int fd = 0; fd < num_fds; fd++) {
            bool r = read_set && FD_ISSET(fd, read_set);
            bool w = write_set && FD_ISSET(fd, write_set);
            bool e = except_set && FD_ISSET(fd, except_set);
            if (r || w || e) {
                pfds[idx].fd = fd;
                pfds[idx].events = 0;
                if (r) pfds[idx].events |= POLLIN;
                if (w) pfds[idx].events |= POLLOUT;
                if (e) pfds[idx].events |= POLLPRI;
                pfds[idx].revents = 0;
                idx++;
            }
        }
    }

    int poll_timeout = -1;
    if (timeout) {
        poll_timeout = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
        if (poll_timeout == 0 && (timeout->tv_sec > 0 || timeout->tv_nsec > 0)) {
            poll_timeout = 1;
        }
    }

    int rc;
    while ((rc = (int)_sc4(BORED_SYS_FS, (uint64_t)FS_CMD_POLL, (uint64_t)pfds, (uint64_t)count, (uint64_t)poll_timeout)) == -2);

    if (read_set) FD_ZERO(read_set);
    if (write_set) FD_ZERO(write_set);
    if (except_set) FD_ZERO(except_set);

    int ready_events = 0;
    if (rc > 0 && pfds) {
        for (int i = 0; i < count; i++) {
            int fd = pfds[i].fd;
            if (pfds[i].events & POLLIN) {
                if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
                    if (read_set) { FD_SET(fd, read_set); ready_events++; }
                }
            }
            if (pfds[i].events & POLLOUT) {
                if (pfds[i].revents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) {
                    if (write_set) { FD_SET(fd, write_set); ready_events++; }
                }
            }
            if (pfds[i].events & POLLPRI) {
                if (pfds[i].revents & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)) {
                    if (except_set) { FD_SET(fd, except_set); ready_events++; }
                }
            }
        }
    } else if (rc < 0) {
        return EIO;
    }

    *num_events = ready_events;
    return 0;
}

} // namespace mlibc
