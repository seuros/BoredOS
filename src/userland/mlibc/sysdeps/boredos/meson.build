# BoredOS mlibc sysdep
# Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
# GPL v3.0

sysdep_supported_options = {
	'posix': true,
}

# ---- include dirs ----
boredos_include_dirs = [
	include_directories('include'),
]
rtld_include_dirs += boredos_include_dirs
libc_include_dirs += boredos_include_dirs

# ---- sources ----
libc_sources += files(
	'generic/sysdeps.cpp',
	'generic/entry.cpp',
)

# ---- ABI headers ----
if not no_headers
	install_headers(
		'include/abi-bits/auxv.h',
		'include/abi-bits/blkcnt_t.h',
		'include/abi-bits/blksize_t.h',
		'include/abi-bits/clockid_t.h',
		'include/abi-bits/dev_t.h',
		'include/abi-bits/errno.h',
		'include/abi-bits/fcntl.h',
		'include/abi-bits/gid_t.h',
		'include/abi-bits/ino_t.h',
		'include/abi-bits/limits.h',
		'include/abi-bits/mode_t.h',
		'include/abi-bits/nlink_t.h',
		'include/abi-bits/pid_t.h',
		'include/abi-bits/seek-whence.h',
		'include/abi-bits/signal.h',
		'include/abi-bits/stat.h',
		'include/abi-bits/uid_t.h',
		'include/abi-bits/vm-flags.h',
		'include/abi-bits/wait.h',
		'include/abi-bits/sigevent.h',
		'include/abi-bits/sigval.h',
		'include/abi-bits/sa_family_t.h',
		'include/abi-bits/sockaddr_storage.h',
		'include/abi-bits/sig-limits.h',
		'include/abi-bits/suseconds_t.h',
		'include/abi-bits/access.h',
		'include/abi-bits/socklen_t.h',
		'include/abi-bits/socket.h',
		'include/abi-bits/poll.h',
		'include/abi-bits/resource.h',
		'include/abi-bits/in.h',
		'include/abi-bits/rlim_t.h',
		'include/abi-bits/utsname.h',
		'include/abi-bits/fd_set.h',
		'include/abi-bits/sem.h',
		'include/abi-bits/time.h',
		'include/abi-bits/ipc.h',
		'include/abi-bits/statvfs.h',
		'include/abi-bits/fsblkcnt_t.h',
		'include/abi-bits/fsfilcnt_t.h',
		'include/abi-bits/shm.h',
		'include/abi-bits/termios.h',
		'include/abi-bits/msg.h',
		'include/abi-bits/mqueue.h',
		'include/abi-bits/utmp-defines.h',
		'include/abi-bits/utmpx.h',
		'include/abi-bits/sigset_t.h',
		subdir: 'abi-bits',
		follow_symlinks: true,
	)
endif

# ---- crt1.o ----
if not headers_only
	c_compiler = meson.get_compiler('c')
	custom_target('crt1',
		build_by_default: true,
		command: c_compiler.cmd_array() + [
			'-m64', '-march=x86-64', '-mno-red-zone', '-ffreestanding',
			'-fno-stack-protector', '-c', '-o', '@OUTPUT@', '@INPUT@'
		],
		input: 'x86_64/crt1.S',
		output: 'crt1.o',
		install: true,
		install_dir: get_option('libdir'),
	)
endif
