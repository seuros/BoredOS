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

extern "C" void *__dso_handle = nullptr;

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
    (void)fsfdt; (void)fd; (void)path; (void)flags;
    if (!result) return EFAULT;
    __builtin_memset(result, 0, sizeof(*result));
    result->st_mode = 0100644;
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
    (void)child;
    return ENOSYS;
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
    (void)path;
    *handle = -1;
    return ENOSYS;
}

int SysdepImpl<ReadEntries>::operator()(int handle, void *buffer, size_t max_size, size_t *bytes_read) {
    (void)handle; (void)buffer; (void)max_size;
    *bytes_read = 0;
    return ENOSYS;
}

} // namespace mlibc
