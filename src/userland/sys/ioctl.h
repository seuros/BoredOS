#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/* ioctl command codes */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOPAN_DISPLAY     0x4604
#define FBIOGETCMAP         0x4604
#define FBIOPUTCMAP         0x4605

/* Framebuffer variable screen information */
struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct {
        uint32_t offset;
        uint32_t length;
    } red, green, blue, transp;
    uint32_t nonstd;
    uint32_t activate;
    uint32_t height;
    uint32_t width;
    uint32_t accel_flags;
    uint32_t pixclock;
    uint32_t left_margin;
    uint32_t right_margin;
    uint32_t upper_margin;
    uint32_t lower_margin;
    uint32_t hsync_len;
    uint32_t vsync_len;
    uint32_t sync;
    uint32_t vmode;
    uint32_t rotate;
    uint32_t colorspace;
    uint32_t reserved[4];
};

/* Framebuffer fixed screen information */
struct fb_fix_screeninfo {
    char id[16];
    uint64_t smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t type_aux;
    uint32_t visual;
    uint16_t xpanstep;
    uint16_t ypanstep;
    uint16_t ywrapstep;
    uint32_t line_length;
    uint64_t mmio_start;
    uint32_t mmio_len;
    uint32_t accel;
    uint16_t reserved[3];
};

/* Visual types */
#define FB_VISUAL_MONO01          0
#define FB_VISUAL_MONO10          1
#define FB_VISUAL_TRUECOLOR       2
#define FB_VISUAL_PSEUDOCOLOR     3
#define FB_VISUAL_DIRECTCOLOR     4
#define FB_VISUAL_STATIC_PSEUDOCOLOR 5
#define FB_VISUAL_FOURCC          6

/* Framebuffer type */
#define FB_TYPE_PACKED_PIXELS     0
#define FB_TYPE_PLANES            1
#define FB_TYPE_INTERLEAVED_PLANES 2
#define FB_TYPE_TEXT              3
#define FB_TYPE_VGA_PLANES        4
#define FB_TYPE_SUPERIMPOSE       5
#define FB_TYPE_FOURCC            6

/* ioctl syscall wrapper - implemented in posix_io.c */
int ioctl(int fd, unsigned long request, ...);

#endif /* _SYS_IOCTL_H */
