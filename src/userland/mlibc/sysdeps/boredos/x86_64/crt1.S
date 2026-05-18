// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// GPL v3.0 — BoredOS mlibc port crt1.S (x86_64)
//
// ELF entry point for statically-linked BoredOS userspace programs.
// On entry: %rsp points to the System V ABI initial stack (argc, argv, envp,
// auxv) as loaded by the BoredOS ELF loader.
// We call __mlibc_entry(entry_stack*, main) which handles argc/argv/environ
// setup and then invokes main().

.section .text
.global _start
.type   _start, @function
_start:
    // Clear frame pointer — outermost stack frame
    xor  %rbp, %rbp

    // arg0: pointer to the initial stack (ABI entry_stack)
    mov  %rsp, %rdi

    // arg1: address of main()
    lea  main(%rip), %rsi

    // Align stack to 16 bytes before the call (System V ABI requirement)
    and  $~0xf, %rsp

    call __mlibc_entry

    // Should never be reached; terminate if it does
    ud2
.size _start, . - _start
