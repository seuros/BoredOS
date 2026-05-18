// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// GPL v3.0 — BoredOS mlibc sysdep tag declarations (sysdeps.hpp)
//
// Declares which mlibc sysdep "tags" the BoredOS backend implements.
// mlibc uses these to select which standard library functions are available.
#pragma once

#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct BoredOSSysdepTags :
    // --- Mandatory sysdeps ---
    LibcPanic,
    LibcLog,
    Exit,
    FutexWait,
    FutexWake,
    TcbSet,
    AnonAllocate,
    AnonFree,
    VmMap,
    VmUnmap,
    ClockGet,
    Sleep,
    // --- File / IO ---
    Open,
    Read,
    Write,
    Seek,
    Close,
    Isatty,
    // --- POSIX ---
    Dup,
    Dup2,
    Pipe,
    Fcntl,
    Ioctl,
    GetCwd,
    Chdir,
    Mkdir,
    Rmdir,
    Unlinkat,
    Stat,
    Access,
    // --- Process ---
    Fork,
    Execve,
    Waitpid,
    Kill,
    Sigaction,
    Sigprocmask,
    // --- Directory ---
    OpenDir,
    ReadEntries
{};

template<typename Tag>
using Sysdeps = SysdepOf<BoredOSSysdepTags, Tag>;

} // namespace mlibc
