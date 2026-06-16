# Syscall Reference

This page documents the current syscall surface in BoredOS as implemented in:
- `src/sys/syscall.h` (kernel command IDs)
- `external/libc/include/syscall.h` (userland wrappers)

Use libc wrappers when possible instead of calling raw syscall numbers directly.

## Top-Level Syscall Numbers

| Number | Name | Purpose |
|---|---|---|
| 0 | `SYS_EXIT` (userland header) | Terminate current process |
| 1 | `SYS_WRITE` | Write to stdout/tty path |
| 3 | `SYS_GUI` | Defunct (previously Window Manager) |
| 4 | `SYS_FS` | Filesystem and fd commands |
| 5 | `SYS_SYSTEM` | System-wide command multiplexer |
| 8 | `SYS_DEBUG_SERIAL` | Debug serial output (kernel only) |
| 9 | `SYS_SBRK` (userland header) | Heap break management |
| 10 | `SYS_KILL` (userland header) | Kill process by PID |
| 60 | `SYS_EXIT` (kernel header) | Internal kernel syscall number map |

Notes:
- Some numbers differ between kernel and userland headers for historical reasons. For app code, rely on wrapper functions in `external/libc/src/syscall.c`.
- `SYS_FS` and `SYS_SYSTEM` are command multiplexers.

## FS Command IDs (`SYS_FS`)

| ID | Macro | Meaning |
|---|---|---|
| 1 | `FS_CMD_OPEN` | Open file |
| 2 | `FS_CMD_READ` | Read from fd |
| 3 | `FS_CMD_WRITE` | Write to fd |
| 4 | `FS_CMD_CLOSE` | Close fd |
| 5 | `FS_CMD_SEEK` | Seek in file |
| 6 | `FS_CMD_TELL` | Current offset |
| 7 | `FS_CMD_LIST` | Directory listing |
| 8 | `FS_CMD_DELETE` | Delete file |
| 9 | `FS_CMD_SIZE` | File size |
| 10 | `FS_CMD_MKDIR` | Create directory |
| 11 | `FS_CMD_EXISTS` | Path exists check |
| 12 | `FS_CMD_GETCWD` | Get cwd |
| 13 | `FS_CMD_CHDIR` | Change cwd |
| 14 | `FS_CMD_GET_INFO` | File metadata |
| 15 | `FS_CMD_DUP` | `dup` fd |
| 16 | `FS_CMD_DUP2` | `dup2` fd |
| 17 | `FS_CMD_PIPE` | Create pipe |
| 18 | `FS_CMD_FCNTL` | `fcntl` flags ops |
| 19 | `FS_CMD_STATFS` | Get filesystem statistics |
| 20 | `FS_CMD_MOUNT_COUNT` | Get number of active mounts |
| 21 | `FS_CMD_MOUNT_INFO` | Get mount metadata |
| 22 | `FS_CMD_POLL` | Wait for events on multiple fds |
| 23 | `FS_CMD_SELECT` | Multiplexed I/O (stub/alias) |

## SYSTEM Command IDs (`SYS_SYSTEM`)

### Desktop and display

| ID | Macro | Meaning |
|---|---|---|
| 29 | `SYSTEM_CMD_SET_TEXT_COLOR` | Set console text color |
| 49 | `SYSTEM_CMD_SET_KEYBOARD_LAYOUT` | Set active keyboard layout ID |
| 51 | `SYSTEM_CMD_GET_KEYBOARD_LAYOUT` | Get current keyboard layout ID |

### Time, power, and system state

| ID | Macro | Meaning |
|---|---|---|
| 11 | `SYSTEM_CMD_RTC_GET` | Read RTC datetime |
| 12 | `SYSTEM_CMD_REBOOT` | Reboot machine |
| 13 | `SYSTEM_CMD_SHUTDOWN` | Power off machine |
| 15 | `SYSTEM_CMD_GET_MEM_INFO` | Return total/used memory |
| 16 | `SYSTEM_CMD_GET_TICKS` | Return scheduler tick count |
| 32 | `SYSTEM_CMD_RTC_SET` | Set RTC datetime |
| 41 | `SYSTEM_CMD_SET_RAW_MODE` | Terminal raw-mode control |
| 43 | `SYSTEM_CMD_YIELD` | Yield scheduler timeslice |
| 46 | `SYSTEM_CMD_SLEEP` | Sleep current process |



### Process, tty, signals

| ID | Macro | Meaning |
|---|---|---|
| 50 | `SYSTEM_CMD_PARALLEL_RUN` | Dispatch parallel job |
| 60 | `SYSTEM_CMD_TTY_CREATE` | Create tty |
| 61 | `SYSTEM_CMD_TTY_READ_OUT` | Read tty output buffer |
| 62 | `SYSTEM_CMD_TTY_WRITE_IN` | Write tty input buffer |
| 63 | `SYSTEM_CMD_TTY_READ_IN` | Read input for current tty |
| 64 | `SYSTEM_CMD_SPAWN` | Spawn process |
| 65 | `SYSTEM_CMD_TTY_SET_FG` | Set tty foreground PID |
| 66 | `SYSTEM_CMD_TTY_GET_FG` | Get tty foreground PID |
| 67 | `SYSTEM_CMD_TTY_KILL_FG` | Kill tty foreground PID |
| 68 | `SYSTEM_CMD_TTY_KILL_ALL` | Kill tty process group |
| 69 | `SYSTEM_CMD_TTY_DESTROY` | Destroy tty |
| 70 | `SYSTEM_CMD_EXEC` | Exec replace current process |
| 71 | `SYSTEM_CMD_WAITPID` | Wait/reap child |
| 72 | `SYSTEM_CMD_KILL_SIGNAL` | Send signal |
| 73 | `SYSTEM_CMD_SIGACTION` | Set/get handler |
| 74 | `SYSTEM_CMD_SIGPROCMASK` | Signal mask ops |
| 75 | `SYSTEM_CMD_SIGPENDING` | Get pending signals |

### ELF app metadata

| ID | Macro | Meaning |
|---|---|---|
| 76 | `SYSTEM_CMD_GET_ELF_METADATA` | Read full app metadata from an ELF |
| 77 | `SYSTEM_CMD_GET_ELF_PRIMARY_IMAGE` | Read primary icon path from an ELF |

### Disk Management

| ID | Macro | Meaning |
|---|---|---|
| 100 | `SYSTEM_CMD_DISK_GET_COUNT` | Get number of detected disks |
| 101 | `SYSTEM_CMD_DISK_GET_INFO` | Get metadata for a specific disk/partition |
| 102 | `SYSTEM_CMD_DISK_WRITE_GPT` | Write GPT partition table to disk |
| 103 | `SYSTEM_CMD_DISK_WRITE_MBR` | Write MBR partition table to disk |
| 104 | `SYSTEM_CMD_DISK_MKFS_FAT32` | Format a partition as FAT32 |
| 105 | `SYSTEM_CMD_DISK_MOUNT` | Mount a filesystem |
| 106 | `SYSTEM_CMD_DISK_UMOUNT` | Unmount a filesystem |
| 107 | `SYSTEM_CMD_DISK_RESCAN` | Rescan disk for partition changes |
| 108 | `SYSTEM_CMD_DISK_REPLACE_KERNEL` | Copy new kernel to ESP / boot partition |
| 109 | `SYSTEM_CMD_DISK_SYNC` | Flush disk caches for a mountpoint |

## Event-Driven I/O: `sys_poll`

BoredOS supports efficient event-driven I/O via the `sys_poll` syscall. This mechanism allows a process to sleep until one or more file descriptors become "ready" (e.g., data is available to read), rather than spinning in a busy-loop.

### Usage

```c
#include <syscall.h>

struct pollfd fds[1];
fds[0].fd = 0;          // stdin / tty
fds[0].events = POLLIN; // Wait for data to read

// Wait up to 1000ms. Use -1 for infinite wait.
int ready = sys_poll(fds, 1, 1000);

if (ready > 0) {
    if (fds[0].revents & POLLIN) {
        // Data is ready to be read from fd 0
    }
} else if (ready == 0) {
    // Timeout reached
}
```

### Wait Queues

Unlike simpler systems that use periodic polling, BoredOS uses a **Wait Queue** infrastructure:

1. **Registration**: When `sys_poll` is called, the kernel registers the calling process with the wait queues of all requested file descriptors (e.g., the TTY input queue or a Pipe's read queue).
2. **Blocking**: If no events are immediately available, the kernel sets the process state to `PROC_STATE_BLOCKED` and returns a special status code `-2` to the syscall layer.
3. **Waking**: When a hardware interrupt (like a key press) or another process (writing to a pipe) adds data, the resource calls `wait_queue_wake_all()`. This sets all registered processes back to `PROC_STATE_RUNNING`.
4. **Resumption**: The libc wrapper for `sys_poll` detects the `-2` return code and automatically re-invokes the syscall until a final result is ready.

This ensures that idle applications consume **zero CPU cycles** while waiting for input.

## Common Wrapper API (`external/libc/include/syscall.h`)

Typical wrappers used by apps:
- Process/system: `sys_exit`, `sys_yield`, `sys_system` (with `SYSTEM_CMD_SLEEP`), `sys_spawn`, `sys_exec`, `sys_waitpid`
- Filesystem: `sys_open`, `sys_read`, `sys_write_fs`, `sys_close`, `sys_seek`, `sys_tell`, `sys_size`, `sys_list`, `sys_poll`
- Network: `sys_network_init`, `sys_network_dhcp_acquire`, `sys_udp_send`, `sys_tcp_connect`, `sys_tcp_recv_nb`, `sys_dns_lookup`
- TTY: `sys_tty_create`, `sys_tty_read_out`, `sys_tty_write_in`, `sys_tty_set_fg`
- ELF metadata: `sys_get_elf_metadata`, `sys_get_elf_primary_image` — see [`elf_metadata.md`](elf_metadata.md) for full usage

## Best Practices

- Do not hardcode numeric command IDs in app code.
- Prefer high-level libc calls (`open`, `read`, `waitpid`, `sigaction`) where available.
- Use `syscall.h` macros when a raw `sys_system` call is still needed.
