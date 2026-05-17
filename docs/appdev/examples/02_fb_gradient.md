<div align="center">
  <h1>Example 02: Framebuffer Gradient</h1>
  <p><em>Direct hardware graphics rendering via /dev/fb0 and console mode control.</em></p>
</div>

---

This intermediate example demonstrates how userland applications can open the standard `/dev/fb0` framebuffer device, request display parameters, prevent the kernel's text console from drawing over their pixels, and draw a smooth animated gradient directly to the screen.

## 📝 Concepts Introduced
* Accessing the raw Linux-compatible framebuffer device (`/dev/fb0`).
* Requesting screen specifications using `FBIOGET_VSCREENINFO` and `FBIOGET_FSCREENINFO`.
* Disabling console text blitting (`KD_GRAPHICS`) to prevent terminal characters from clobbering your graphics.
* Performing manual double-buffering by writing a full frame using `write` and `lseek`.
* Restoring the standard text console (`KD_TEXT`) on exit.

---

## The Code (`fb_gradient.c`)

```c
// BOREDOS_APP_DESC: Framebuffer Gradient - draws an animated gradient using /dev/fb0.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <stdint.h>

int main(void) {
    // 1. Open the framebuffer device
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("Error: cannot open /dev/fb0 device node!\n");
        return 1;
    }

    // 2. Query the screen resolution and configuration
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        printf("Error: failed to query framebuffer screen info.\n");
        close(fb_fd);
        return 1;
    }

    printf("Display Details:\n");
    printf("  Resolution: %dx%d (%d bpp)\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("  Line Pitch: %d bytes\n", finfo.line_length);
    printf("  Physical Memory Start: 0x%lx\n", finfo.smem_start);
    printf("  Memory Size: %u bytes\n", finfo.smem_len);

    // 3. Set the active TTY console (fd 0) to Graphics Mode
    // This turns off TTY text rendering, preventing the terminal shell from clobbering our drawings.
    if (ioctl(0, KDSETMODE, (void*)KD_GRAPHICS) < 0) {
        printf("Warning: failed to set TTY to graphics mode.\n");
    }

    // 4. Allocate a backbuffer for smooth rendering
    uint32_t screen_size = finfo.line_length * vinfo.yres;
    uint8_t *backbuffer = malloc(screen_size);
    if (!backbuffer) {
        printf("Error: failed to allocate backbuffer!\n");
        ioctl(0, KDSETMODE, (void*)KD_TEXT);
        close(fb_fd);
        return 1;
    }

    // 5. Draw an animated color gradient loop
    printf("Drawing gradient... Press Ctrl+C inside TTY to exit (or wait for the animation to end).\n");
    
    for (int frame = 0; frame < 120; frame++) {
        for (uint32_t y = 0; y < vinfo.yres; y++) {
            // Locate the start of this pixel row in the backbuffer
            uint32_t *row = (uint32_t *)(backbuffer + y * finfo.line_length);
            
            for (uint32_t x = 0; x < vinfo.xres; x++) {
                // Generate a moving color gradient
                uint8_t red   = (x * 255 / vinfo.xres) + frame;
                uint8_t green = (y * 255 / vinfo.yres) - frame;
                uint8_t blue  = frame * 2;
                
                // Pack as BGRA32 pixel (0xAARRGGBB or 0xBBGGRRXX depending on endianness/layout)
                row[x] = (blue) | (green << 8) | (red << 16) | (0xFF << 24);
            }
        }

        // Write the backbuffer directly to /dev/fb0 screen memory
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, screen_size);

        // Sleep for a short duration (approx 30fps)
        sleep(33);
    }

    // 6. Cleanup and RESTORE TTY Console Text Mode
    // IMPORTANT: If we don't restore KD_TEXT, the terminal will remain frozen in black/graphics mode!
    free(backbuffer);
    ioctl(0, KDSETMODE, (void*)KD_TEXT);
    close(fb_fd);

    printf("Returned to standard text terminal mode.\n");
    return 0;
}
```

---

## How it Works

1.  **Opening `/dev/fb0`**: `/dev/fb0` is the standard Linux device file for the graphics framebuffer. Under BoredOS, opening this node routes operations directly to the physical display memory allocated by the Limine bootloader.
2.  **`ioctl` Configuration Queries**: 
    - `ioctl(..., FBIOGET_VSCREENINFO, &vinfo)` queries the *variable* screen settings, returning the active `xres`, `yres` (e.g. 1024x768), and `bits_per_pixel` (32 bpp).
    - `ioctl(..., FBIOGET_FSCREENINFO, &finfo)` queries the *fixed* hardware settings, specifically returning the `line_length` (row pitch in bytes) which accounts for memory alignment padding, and `smem_start` (the physical memory start address).
3.  **TTY Mode Switching (`KDSETMODE`)**:
    - **`KD_GRAPHICS`**: When set, this disables the kernel scheduler's active console redraw loop (`g_tty_blit_enabled = false`). This stops the background terminal from drawing ASCII characters over our direct pixel writes.
    - **`KD_TEXT`**: Re-enables the active TTY console blitting loop, allowing standard terminal text input and output to resume normally.
4.  **Offset Math & Seeking**:
    - A 32-bpp display uses 4 bytes per pixel (BGRA).
    - The actual location of a pixel at `(x, y)` in memory is computed as:
      $$\text{offset} = (y \times \text{line\_length}) + (x \times 4)$$
    - Rather than performing single-pixel writes, we construct a full frame in a temporary heap buffer (`backbuffer`) and write it in a single high-performance `write()` operation.
    - Before writing, `lseek(fb_fd, 0, SEEK_SET)` resets the VFS file cursor back to the start of physical screen memory.

## Zero-Copy Rendering via `mmap`

BoredOS also supports mapping `/dev/fb0` directly into your application's virtual address space using `mmap()`:

```c
uint32_t screen_size = finfo.line_length * vinfo.yres;
uint32_t *fb_mmap = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

if (fb_mmap != MAP_FAILED) {
    // You can now write pixels directly into the screen buffer with no overhead!
    fb_mmap[y * (finfo.line_length / 4) + x] = 0xFF00FF00; // Direct draw green pixel
    
    munmap(fb_mmap, screen_size);
}
```

---
