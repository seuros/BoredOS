// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// GPL v3.0 — BoredOS mlibc port: C++ entry point (entry.cpp)
//
// Called from crt1.S as:
//   __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int, char**, char**))
//
// entry_stack points to the System V ABI initial stack layout:
//   [0]        argc
//   [1..argc]  argv pointers
//   [argc+1]   NULL
//   [...]      envp pointers
//   [...]      NULL
//   [...]      auxv entries (AT_NULL terminated)

#include <stdint.h>
#include <stdlib.h>
#include <mlibc/elf/startup.h>

extern "C" void __dlapi_enter(uintptr_t *);
extern char **environ;

extern "C" [[noreturn]]
void __mlibc_entry(uintptr_t *entry_stack,
                   int (*main_fn)(int, char **, char **)) {
    // Performs static-link initialisation: runs global constructors,
    // sets up environ/auxv from the ABI stack.
    __dlapi_enter(entry_stack);

    int result = main_fn(mlibc::entry_stack.argc,
                         mlibc::entry_stack.argv,
                         environ);
    exit(result);
    __builtin_unreachable();
}
