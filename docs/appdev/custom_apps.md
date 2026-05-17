<div align="center">
  <h1>Creating a Custom CLI App</h1>
  <p><em>A step-by-step tutorial on writing a new C userland application.</em></p>
</div>

---

This guide explains how to write a new "Hello World" command-line application locally, compile it as an `.elf` binary into the `bin/` folder, and launch it inside BoredOS.

> [!TIP]
> **Looking for working code?** Check out the [Examples Directory](examples/README.md) for full source code demonstrating basic CLI, direct Framebuffer drawing, and TCP Networking.

## Step 1: Write the C Source

Applications reside in the `src/userland/` directory. Create a new file for a CLI app under `src/userland/cli/hello.c`.

```c
// BOREDOS_APP_DESC: Hello World - my first BoredOS command-line app!
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Hello, BoredOS!!\n");
    
    if (argc > 1) {
        printf("You passed %d arguments. First argument: %s\n", argc - 1, argv[1]);
    } else {
        printf("No arguments passed. Try running: hello world\n");
    }

    return 0; // Returning 0 smoothly exits the process via crt0.asm
}
```

## Step 2: Edit the Makefile

Now you need to tell the build system to compile `hello.c`. Fortunately, the `src/userland/Makefile` is designed to detect new C files in source folders automatically!

1. Open `src/userland/Makefile`.
2. Find the line specifying `APP_SOURCES_FULL`:
   ```make
   APP_SOURCES_FULL = $(wildcard cli/*.c sys/*.c net/*.c *.c)
   ```
   Since you placed the file in `cli/hello.c`, the wildcard logic will pick it up automatically.
3. The Makefile will generate `bin/hello.elf` during the build phase.

## Step 3: Bundle it into the OS

The main overarching `Makefile` (in the project root) takes binaries from `src/userland/bin/*.elf` and places them into the `iso_root/bin/` directory, while also making them available on the ISO boot image.

1. Go back to the root of the OS:
   ```sh
   cd ../..
   ```
2. Compile the entire project to build the ISO and test in QEMU:
   ```sh
   make clean && make run
   ```

## Step 4: Run it inside BoredOS

1. When BoredOS boots, you are automatically logged into the main shell console (TTY0).
2. The OS automatically maps built applications in `/bin` to standard shell commands. Simply type your application's filename (without the `.elf` extension).
3. Type `hello` (or `hello argument1`) in the terminal and press Enter.
4. Your custom application will run, print its output, and exit cleanly!

---