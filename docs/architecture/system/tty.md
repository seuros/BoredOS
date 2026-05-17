# TTY & Virtual Terminals Architecture

BoredOS implements a modular, UNIX-like multi-TTY (Teletype / Virtual Terminal) multiplexer. The graphical desktop of previous versions has been fully replaced with a clean, low-overhead system of **10 virtual terminal consoles** (`/dev/tty0` through `/dev/tty9`).

This document details how these virtual consoles isolate their screens, multiplex user input, and coordinate with the physical display memory.

---

## 1. TTY Structure and Console Isolation

The core structure representing a virtual console is `tty_t`, defined in [`src/sys/tty.h`](../../../src/sys/tty.h). The system statically allocates a table of 10 virtual terminal structures:

```c
tty_t g_ttys[10];
int g_active_tty = 0;       // Currently displayed console index (0-9)
bool g_tty_blit_enabled = true; // Toggle for TTY blitting (Graphics Mode control)
```

### Virtual Framebuffer Allocation (`vfb`)
Unlike systems that write text characters directly to a single shared physical text mode screen, BoredOS runs in high-resolution graphics mode (typically 1024x768). To allow 10 separate consoles to coexist without interfering with each other, **each TTY is completely isolated in memory**.

1. At system startup, `tty_init()` is called during kernel initialization.
2. For each of the 10 terminals, the kernel queries the graphics screen size (`width * height * 4` bytes).
3. The kernel allocates a dedicated **virtual framebuffer (VFB)** using `kmalloc()` on the kernel heap:
   ```c
   t->vfb = (uint32_t *)kmalloc(width * height * 4);
   memset(t->vfb, 0, width * height * 4);
   ```
4. Each TTY has its own coordinates, text styling (colors), character grid buffer, and ANSI escape sequence parsing state.
5. When a process writes text (or ANSI escape sequences) to a TTY device node (e.g., via `printf`), the kernel renders characters as pixel glyphs **directly into that TTY's private `vfb` buffer**, leaving other consoles completely unaffected.

---

## 2. Active Screen Blitting

The kernel contains a background display coordination routine called `tty_blit_active()`, which runs periodically (usually driven by the timer interrupt or graphics display thread):

```c
void tty_blit_active(void) {
    if (!g_tty_blit_enabled) {
        return; // Graphics mode is active; do not write to screen
    }

    tty_t *active = &g_ttys[g_active_tty];
    if (active && active->vfb) {
        // Fast copy from active TTY's private VFB to the real physical screen
        graphics_copy_buffer(active->vfb);
    }
}
```

### Active TTY Switching
When the user switches active terminals (e.g. by pressing key combinations or via kernel trigger):
1. The kernel updates the `g_active_tty` index (e.g. `g_active_tty = 1`).
2. On the next display tick, `tty_blit_active()` copies `g_ttys[1].vfb` to the screen instead.
3. The display transitions instantly, restoring the exact visual state of the newly activated console!

---

## 3. Disabling Blitting: Graphics Mode (`KDSETMODE`)

When a userspace application wants to draw directly to `/dev/fb0` (e.g., to run a custom game, GUI compositor, or image viewer), the background TTY text blitting loop would normally overwrite its pixels on the next tick.

To coordinate display ownership, BoredOS implements the standard POSIX console control interface `ioctl(tty_fd, KDSETMODE, mode)`:

- **`KD_GRAPHICS`** (`0x01`): Stops the kernel from rendering terminal text.
  ```c
  g_tty_blit_enabled = false;
  ```
  This freezes TTY console blits, giving the userspace application exclusive access to write raw pixels directly to `/dev/fb0` without interference.
- **`KD_TEXT`** (`0x00`): Re-enables kernel terminal text rendering.
  ```c
  g_tty_blit_enabled = true;
  ```
  Text blitting immediately resumes, copying the active TTY's `vfb` buffer to the display on the next tick.

This mechanism is used by userland tools (like `fbtest`) to safely draw custom graphics.

---

## 4. Input Multiplexing and Event Routing

Physical input devices (PS/2 Keyboard and PS/2 Mouse) generate hardware interrupts that must be distributed to the correct terminal.

1. **Hardware Interrupt**: The user presses a key, triggering the keyboard interrupt handler (`src/dev/keyboard.c`).
2. **Key Decoding**: The handler translates raw hardware scancodes into decoded key characters (and handles key modifiers like Ctrl, Shift, Alt).
3. **Active TTY Check**: The kernel identifies the currently active terminal index `g_active_tty`.
4. **Queue Insertion**: The decoded character is pushed into the active TTY's input ring buffer queue (`active_tty->input_queue`).
5. **Wakeup**: If a shell or user process was blocked waiting for keyboard input on that TTY, the wait queue is woken up, and the `read()` call returns the new characters.

This design ensures that typing only affects the visible console, providing complete multi-session safety.

---

## 5. TTY Device Nodes in the VFS

The 10 virtual terminals are registered in the Virtual File System (VFS) as character devices under:
- `/dev/tty0` through `/dev/tty9`
- `/dev/tty` (a symbolic alias device node that always points to the calling process's controlling TTY)

When a process is spawned by the shell:
1. The kernel copies the file descriptors of the controlling TTY to the new process's file descriptor table (mapping standard input `0`, standard output `1`, and standard error `2`).
2. Standard I/O operations are seamlessly routed via the VFS to the corresponding TTY's input and output queues, providing a standard, transparent Unix-like console interface.

---
