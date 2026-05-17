# Linux-Compatible Framebuffer Device Support for BoredOS

## Overview

BoredOS now exposes the framebuffer as a Linux-compatible character device at `/dev/fb0`. This enables userland applications (including graphics libraries like tinyx) to read/write raw framebuffer pixel data and query framebuffer parameters using standard Linux framebuffer ioctl commands.

## Features

✅ **Linux-Compatible `/dev/fb0` Device**
- Read/write access to framebuffer memory
- Support for seeking within framebuffer
- Standard Linux framebuffer ioctl commands
- Appears in `/dev/` directory listing
- Registered as character device 29 in `/proc/devices`

✅ **Framebuffer Operations**
- **Read**: Stream raw pixel data from framebuffer
- **Write**: Stream raw pixel data to framebuffer
- **Seek**: Position within framebuffer for partial reads/writes
- **Ioctl**: Query framebuffer parameters

✅ **Linux Framebuffer Ioctl Commands**
- `FBIOGET_VSCREENINFO` (0x4600): Get variable screen info
  - Resolution (xres, yres)
  - Virtual resolution
  - Bits per pixel
  - RGB color component bit offsets and sizes
  
- `FBIOGET_FSCREENINFO` (0x4602): Get fixed screen info
  - Physical framebuffer memory address
  - Framebuffer total memory size
  - Line length (pitch)
  - Memory model type

## Implementation Details

### Device Registration

The framebuffer device is automatically registered when the kernel initializes the graphics system. It appears as:
- Device file: `/dev/fb0`
- Device type: Character device (type 4 in BoredOS VFS)
- Major number: 29 (Linux standard)
- Mode: Read/Write access

### File Operations

#### Opening `/dev/fb0`
```c
int fd = open("/dev/fb0", O_RDWR);
// or in userland: FILE* fb = fopen("/dev/fb0", "r+");
```

#### Reading Pixel Data
```c
// Read entire framebuffer
uint8_t buffer[width * height * 4];
lseek(fd, 0, SEEK_SET);
read(fd, buffer, sizeof(buffer));

// Or read specific region
uint64_t offset = y * pitch + x * bytes_per_pixel;
lseek(fd, offset, SEEK_SET);
read(fd, pixel_data, size);
```

#### Writing Pixel Data
```c
// Write entire framebuffer
lseek(fd, 0, SEEK_SET);
write(fd, buffer, width * height * 4);

// Or write specific region
uint64_t offset = y * pitch + x * bytes_per_pixel;
lseek(fd, offset, SEEK_SET);
write(fd, pixel_data, size);
```

#### Querying Framebuffer Properties
```c
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
ioctl(fd, FBIOGET_FSCREENINFO, &finfo);

printf("Resolution: %ux%u @ %u bpp\n", 
       vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
printf("Framebuffer at 0x%lx, %u bytes\n", 
       finfo.smem_start, finfo.smem_len);
```

### Data Structures

#### fb_var_screeninfo (Variable Screen Info)
```c
struct fb_var_screeninfo {
    uint32_t xres;              // Visible resolution
    uint32_t yres;
    uint32_t xres_virtual;      // Virtual resolution
    uint32_t yres_virtual;
    uint32_t xoffset;           // Offset (panning)
    uint32_t yoffset;
    uint32_t bits_per_pixel;    // Color depth
    uint32_t grayscale;         // 0 = color, 1 = grayscale
    struct {                    // Color component layout
        uint32_t offset;
        uint32_t length;
    } red, green, blue, transp; // RGB transparency
    uint32_t nonstd;            // Non-standard mode flag
    uint32_t activate;          // Update on set_var
    uint32_t height;            // Physical height (mm)
    uint32_t width;             // Physical width (mm)
    uint32_t accel_flags;       // Hardware acceleration flags
    // ... additional fields for timing ...
};
```

#### fb_fix_screeninfo (Fixed Screen Info)
```c
struct fb_fix_screeninfo {
    char id[16];                // Framebuffer device name
    uint64_t smem_start;        // Physical start address
    uint32_t smem_len;          // Length of framebuffer memory
    uint32_t type;              // Frame buffer type
    uint32_t type_aux;          // Interleave for interleaved planes
    uint32_t visual;            // Visual type (FB_VISUAL_TRUECOLOR)
    uint16_t xpanstep;          // Horizontal pan step
    uint16_t ypanstep;          // Vertical pan step
    uint16_t ywrapstep;         // Vertical wrap step
    uint32_t line_length;       // Length of line in bytes
    uint64_t mmio_start;        // Memory-mapped I/O address
    uint32_t mmio_len;          // Length of MMIO area
    uint32_t accel;             // Hardware acceleration ID
};
```

## Usage Examples

### Example 1: Query Framebuffer Information
```c
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602

int main() {
    int fd = open("/dev/fb0", O_RDWR);
    
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
    
    printf("Display: %ux%u @ %u bpp\n", 
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("Framebuffer: %u bytes at 0x%lx\n", 
           finfo.smem_len, finfo.smem_start);
    printf("Pitch: %u bytes\n", finfo.line_length);
    
    close(fd);
    return 0;
}
```

### Example 2: Draw a Pixel
```c
int main() {
    int fd = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
    
    int x = 100, y = 100;
    uint32_t pixel = 0xFF0000FF; // Red in BGRA
    
    // Calculate offset: y * pitch + x * bytes_per_pixel
    uint64_t offset = (uint64_t)y * finfo.line_length + 
                      (uint64_t)x * (vinfo.bits_per_pixel / 8);
    
    lseek(fd, offset, SEEK_SET);
    write(fd, &pixel, 4);
    
    close(fd);
    return 0;
}
```

### Example 3: Clear Framebuffer
```c
int main() {
    int fd = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
    
    // Fill with zeros (black)
    uint8_t *buffer = malloc(finfo.smem_len);
    memset(buffer, 0, finfo.smem_len);
    
    lseek(fd, 0, SEEK_SET);
    write(fd, buffer, finfo.smem_len);
    
    free(buffer);
    close(fd);
    return 0;
}
```

## Test Application

A test application `fbtest` is provided to demonstrate framebuffer access:

```bash
# Show framebuffer information
fbtest info

# Clear framebuffer to black
fbtest clear

# Draw a 4-color test pattern
fbtest pattern
```

See `src/userland/apps/fbtest.c` for implementation.

## Architecture & Implementation

### Files Modified

1. **src/fs/vfs.h**
   - Added `DEVICE_TYPE_FRAMEBUFFER` constant

2. **src/graphics/graphics.h**
   - Added `framebuffer_info_t` structure
   - Added framebuffer getter functions

3. **src/graphics/graphics.c**
   - Implemented framebuffer query functions

4. **src/fs/vfs.c**
   - Added framebuffer device handling in:
     - `vfs_open()`: Register /dev/fb0
     - `vfs_read()`: Read framebuffer pixels
     - `vfs_write()`: Write framebuffer pixels
     - `vfs_seek()`: Position within framebuffer
     - `vfs_ioctl()`: Handle framebuffer ioctls
     - `vfs_file_size()`: Return framebuffer size
     - `vfs_list_directory()`: List fb0 in /dev
     - `vfs_exists()`: Check fb0 existence
     - `vfs_is_directory()`: Identify as non-directory

### Design Decisions

- **Direct Memory Access**: Reads/writes directly access the physical framebuffer buffer provided by Limine
- **No Circular Dependencies**: Framebuffer struct is defined locally in vfs.c to avoid including graphics.h
- **Byte-Oriented I/O**: Read/write operations work with bytes for flexibility
- **Seek Support**: Full SEEK_SET/SEEK_CUR/SEEK_END semantics
- **Linux API Compatibility**: Struct layouts and ioctl numbers match Linux kernel exactly

## Limitations & Future Work

### Current Limitations
- Only `/dev/fb0` supported (single framebuffer)
- No hardware acceleration features yet
- No colormap operations (FBIOGETCMAP/FBIOPUTCMAP)
- No panning/scrolling via ioctl (FBIOPAN_DISPLAY)
- No mmap support (zero-copy access)

### Future Enhancements
1. Multiple framebuffer support (/dev/fb1, /dev/fb2)
2. Memory mapping via `mmap()` for zero-copy access
3. Pan/scroll support for smooth animation
4. Colormap operations
5. Hardware acceleration capabilities
6. Blanking/power management
7. Mode setting (resolution changes)

## Linux Software Compatibility

This implementation enables compatibility with:
- **TinyX/Xvfb**: X server implementation for framebuffer
- **Directfb**: Direct framebuffer library
- **SDL/pygame**: Graphics libraries supporting fbdev
- **Custom graphics applications** using standard Linux fbdev API

## Verification

To verify the framebuffer device is working:

```bash
# Check if fb0 exists
ls -la /dev/fb0

# Query framebuffer in /dev listing
ls -la /dev/fb*

# Check /proc/devices
grep "fb" /proc/devices

# Run fbtest
fbtest info
```

Expected output:
```
/dev/fb0: character device, readable and writable
29 fb (in /proc/devices)
Display: 1024x768 @ 32 bpp
Framebuffer: 3145728 bytes at 0x...
Pitch: 4096 bytes
```

## References

- Linux Framebuffer Howto: https://www.tldp.org/HOWTO/Framebuffer-HOWTO.html
- Linux FB Driver Documentation: https://www.kernel.org/doc/html/latest/fb/
- FB ioctl Commands: https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/fb.h
