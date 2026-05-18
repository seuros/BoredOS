// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// GPL v3.0 — BoredOS mlibc port signals.S (x86_64)
//
// Signal trampoline for POSIX signal delivery.
// When the kernel delivers a signal it redirects execution here after the
// handler returns, so the kernel can clean up the signal frame.
// For BoredOS the kernel currently does the return redirect inline, so this
// is a minimal stub.

.section .text
.global __mlibc_signal_restore
.type   __mlibc_signal_restore, @function
__mlibc_signal_restore:
    // No-op trampoline — the kernel handles the return path for now.
    ret
.size __mlibc_signal_restore, . - __mlibc_signal_restore
