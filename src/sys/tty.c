// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
#include "tty.h"
#include "spinlock.h"
#include "wait_queue.h"
#include "wm/font.h"
#include "../mem/memory_manager.h"
#include "../wm/graphics.h"
#include "../core/kutils.h"
#include <stdbool.h>
#include <stdint.h>

static tty_t g_ttys[TTY_COUNT];
static int g_active_tty = 0;
static spinlock_t g_tty_global_lock = SPINLOCK_INIT;

#include "../core/kutils.h"

static void tty_queue_init(tty_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    wait_queue_init(&q->wait_queue);
    memset(q->buffer, 0, TTY_IN_QUEUE_SIZE);
}

static void tty_queue_push(tty_queue_t *q, uint8_t val) {
    uint32_t next = (q->head + 1) % TTY_IN_QUEUE_SIZE;
    if (next != q->tail) {
        q->buffer[q->head] = val;
        q->head = next;
        wait_queue_wake_all(&q->wait_queue);
    }
}

static int tty_queue_pop(tty_queue_t *q, uint8_t *buf, size_t len) {
    size_t count = 0;
    while (q->head != q->tail && count < len) {
        buf[count++] = q->buffer[q->tail];
        q->tail = (q->tail + 1) % TTY_IN_QUEUE_SIZE;
    }
    return (int)count;
}

void tty_init(void) {
    int w = get_screen_width();
    int h = get_screen_height();
    size_t vfb_size = w * h * 4;

    graphics_clear_back_buffer(0xFF000000);
    graphics_mark_screen_dirty();
    graphics_flip_buffer();

    for (int i = 0; i < TTY_COUNT; i++) {
        g_ttys[i].id = i;
        g_ttys[i].used = true;
        g_ttys[i].width = w;
        g_ttys[i].height = h;
        g_ttys[i].vfb = (uint32_t *)kmalloc(vfb_size);
        g_ttys[i].cursor_x = 0;
        g_ttys[i].cursor_y = 0;
        g_ttys[i].cursor_visible = true;
        g_ttys[i].fg_color = 0xFFFFFFFF;
        g_ttys[i].bg_color = 0xFF000000;
        g_ttys[i].fg_pid = -1;
        g_ttys[i].esc_state = 0;
        g_ttys[i].esc_num_params = 0;
        g_ttys[i].saved_x = 0;
        g_ttys[i].saved_y = 0;
        g_ttys[i].lock = SPINLOCK_INIT;
        
        memset(g_ttys[i].vfb, 0, vfb_size);
        for (size_t j = 0; j < vfb_size / 4; j++) g_ttys[i].vfb[j] = 0xFF000000;
        tty_queue_init(&g_ttys[i].key_queue);
        tty_queue_init(&g_ttys[i].mouse_queue);
        tty_queue_init(&g_ttys[i].out_queue);
        tty_queue_init(&g_ttys[i].char_queue);
    }

    g_active_tty = 0;
}

tty_t* tty_get(int id) {
    if (id < 0 || id >= TTY_COUNT) return NULL;
    return &g_ttys[id];
}

void tty_switch(int id) {
    if (id < 0 || id >= TTY_COUNT) return;
    uint64_t flags = spinlock_acquire_irqsave(&g_tty_global_lock);
    g_active_tty = id;
    spinlock_release_irqrestore(&g_tty_global_lock, flags);
}

int tty_get_active_id(void) {
    return g_active_tty;
}

static void tty_draw_rect(tty_t *t, int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > t->width) w = t->width - x;
    if (y + h > t->height) h = t->height - y;
    if (w <= 0 || h <= 0) return;

    for (int i = y; i < y + h; i++) {
        uint32_t *row = &t->vfb[i * t->width + x];
        for (int j = 0; j < w; j++) {
            row[j] = color;
        }
    }
}

static void tty_draw_char(tty_t *t, int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (x < 0 || x + 8 > t->width || y < 0 || y + 8 > t->height) return;
    unsigned char uc = (unsigned char)c;
    if (uc > 127) uc = 0;
    
    const uint8_t *glyph = font8x8_basic[uc];
    for (int row = 0; row < 8; row++) {
        uint32_t *vfb_row = &t->vfb[(y + row) * t->width + x];
        uint8_t glyph_row = glyph[row];
        for (int col = 0; col < 8; col++) {
            if ((glyph_row >> (7 - col)) & 1) {
                vfb_row[col] = fg;
            } else {
                vfb_row[col] = bg;
            }
        }
    }
}

static void tty_draw_cursor(tty_t *t) {
    if (!t->cursor_visible) return;
    if (t->cursor_x < 0 || t->cursor_x + 8 > t->width) return;
    if (t->cursor_y < 0 || t->cursor_y + 8 > t->height) return;
    
    // Draw inverted block cursor
    for (int row = 0; row < 8; row++) {
        uint32_t *vfb_row = &t->vfb[(t->cursor_y + row) * t->width + t->cursor_x];
        for (int col = 0; col < 8; col++) {
            vfb_row[col] = t->fg_color;
        }
    }
}

static void tty_scroll(tty_t *t) {
    int font_h = 8;
    int vfb_size = t->width * t->height * 4;
    
    memmove(t->vfb, t->vfb + (font_h * t->width), vfb_size - (font_h * t->width * 4));
    tty_draw_rect(t, 0, t->height - font_h, t->width, font_h, t->bg_color);
    
    t->cursor_y -= font_h;
}

void tty_write(int id, const char *data, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return;

    uint64_t flags = spinlock_acquire_irqsave(&t->lock);
    
    int old_cursor_y = t->cursor_y;
    int font_w = 8;
    int font_h = 8;
    
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        
        if (t->esc_state == 1) { 
            if (c == '[') {
                t->esc_state = 2;
                t->esc_num_params = 0;
                for (int p = 0; p < 8; p++) t->esc_params[p] = 0;
            } else if (c == 's') { 
                t->saved_x = t->cursor_x;
                t->saved_y = t->cursor_y;
                t->esc_state = 0;
            } else if (c == 'u') { 
                t->cursor_x = t->saved_x;
                t->cursor_y = t->saved_y;
                t->esc_state = 0;
            } else {
                t->esc_state = 0;
            }
            continue;
        } else if (t->esc_state == 2) { // Saw ESC [
            if (c == '?') {
                t->esc_state = 3;  // DEC private mode
                t->esc_num_params = 0;
                for (int p = 0; p < 8; p++) t->esc_params[p] = 0;
                continue;
            }
            if (c >= '0' && c <= '9') {
                t->esc_params[t->esc_num_params] = t->esc_params[t->esc_num_params] * 10 + (c - '0');
            } else if (c == ';') {
                if (t->esc_num_params < 7) t->esc_num_params++;
            } else {
                // Final command character
                if (c == 'K') { // Erase to end of line
                    tty_draw_rect(t, t->cursor_x, t->cursor_y, t->width - t->cursor_x, font_h, t->bg_color);
                } else if (c == 'J') { // Erase in Display
                    int mode = t->esc_params[0];
                    if (mode == 2) { // Entire screen
                        tty_draw_rect(t, 0, 0, t->width, t->height, t->bg_color);
                    } else if (mode == 1) { // From beginning to cursor
                        tty_draw_rect(t, 0, 0, t->width, t->cursor_y, t->bg_color);
                        tty_draw_rect(t, 0, t->cursor_y, t->cursor_x, font_h, t->bg_color);
                    } else { // From cursor to end
                        tty_draw_rect(t, t->cursor_x, t->cursor_y, t->width - t->cursor_x, font_h, t->bg_color);
                        if (t->cursor_y + font_h < t->height) {
                            tty_draw_rect(t, 0, t->cursor_y + font_h, t->width, t->height - (t->cursor_y + font_h), t->bg_color);
                        }
                    }
                } else if (c == 'H' || c == 'f') { // Home / Position
                    int row = t->esc_params[0];
                    int col = t->esc_params[1];
                    if (row > 0) row--; // 1-indexed to 0-indexed
                    if (col > 0) col--;
                    t->cursor_x = col * font_w;
                    t->cursor_y = row * font_h;
                    // Clamp
                    if (t->cursor_x >= t->width) t->cursor_x = t->width - font_w;
                    if (t->cursor_y >= t->height) t->cursor_y = t->height - font_h;
                } else if (c == 'A') { // Up
                    int n = t->esc_params[0]; if (n == 0) n = 1;
                    t->cursor_y -= n * font_h;
                    if (t->cursor_y < 0) t->cursor_y = 0;
                } else if (c == 'B') { // Down
                    int n = t->esc_params[0]; if (n == 0) n = 1;
                    t->cursor_y += n * font_h;
                    if (t->cursor_y >= t->height) t->cursor_y = t->height - font_h;
                } else if (c == 'C') { // Forward/Right
                    int n = t->esc_params[0]; if (n == 0) n = 1;
                    t->cursor_x += n * font_w;
                    if (t->cursor_x >= t->width) t->cursor_x = t->width - font_w;
                } else if (c == 'D') { // Backward/Left
                    int n = t->esc_params[0]; if (n == 0) n = 1;
                    t->cursor_x -= n * font_w;
                    if (t->cursor_x < 0) t->cursor_x = 0;
                } else if (c == 'm') { // SGR (Color)
                    for (int j = 0; j <= t->esc_num_params; j++) {
                        int p = t->esc_params[j];
                        if (p == 0) { t->fg_color = 0xFFFFFFFF; t->bg_color = 0xFF000000; }
                        else if (p == 1) { /* Bold */ }
                        else if (p >= 30 && p <= 37) {
                            static const uint32_t colors[] = {
                                0xFF000000, 0xFFFF4444, 0xFF6A9955, 0xFFFFCC00,
                                0xFF569CD6, 0xFFC586C0, 0xFF4EC9B0, 0xFFFFFFFF
                            };
                            t->fg_color = colors[p - 30];
                        } else if (p == 38) { // Extended FG
                            if (j + 2 <= t->esc_num_params && t->esc_params[j+1] == 5) { // 256 color
                                uint8_t color_index = (uint8_t)t->esc_params[j+2];
                                // Basic 256 color to RGB
                                if (color_index < 16) {
                                     static const uint32_t colors[] = {
                                        0xFF000000, 0xFF800000, 0xFF008000, 0xFF808000,
                                        0xFF000080, 0xFF800080, 0xFF008080, 0xFFC0C0C0,
                                        0xFF808080, 0xFFFF0000, 0xFF00FF00, 0xFFFFFF00,
                                        0xFF0000FF, 0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF
                                     };
                                     t->fg_color = colors[color_index];
                                } else {
                                     t->fg_color = 0xFFCCCCCC; // Fallback
                                }
                                j += 2;
                            } else if (j + 4 <= t->esc_num_params && t->esc_params[j+1] == 2) { // TrueColor
                                t->fg_color = 0xFF000000 | (t->esc_params[j+2] << 16) | (t->esc_params[j+3] << 8) | t->esc_params[j+4];
                                j += 4;
                            }
                        } else if (p == 39) { t->fg_color = 0xFFFFFFFF; }
                        else if (p >= 40 && p <= 47) {
                            static const uint32_t colors[] = {
                                0xFF000000, 0xFFFF4444, 0xFF6A9955, 0xFFFFCC00,
                                0xFF569CD6, 0xFFC586C0, 0xFF4EC9B0, 0xFFFFFFFF
                            };
                            t->bg_color = colors[p - 40];
                        } else if (p == 48) { // Extended BG
                             if (j + 2 <= t->esc_num_params && t->esc_params[j+1] == 5) {
                                j += 2; // Ignore for now
                            } else if (j + 4 <= t->esc_num_params && t->esc_params[j+1] == 2) {
                                t->bg_color = 0xFF000000 | (t->esc_params[j+2] << 16) | (t->esc_params[j+3] << 8) | t->esc_params[j+4];
                                j += 4;
                            }
                        } else if (p == 49) { t->bg_color = 0xFF000000; }
                        else if (p >= 90 && p <= 97) { // Bright FG
                            static const uint32_t colors[] = {
                                0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
                                0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF
                            };
                            t->fg_color = colors[p - 90];
                        }
                    }
                } else if (c == 's') { // Save cursor
                    t->saved_x = t->cursor_x;
                    t->saved_y = t->cursor_y;
                } else if (c == 'u') { // Restore cursor
                    t->cursor_x = t->saved_x;
                    t->cursor_y = t->saved_y;
                }
                t->esc_state = 0;
            }
            continue;
        } else if (t->esc_state == 3) { // Saw ESC [ ?  (DEC private mode)
            if (c >= '0' && c <= '9') {
                t->esc_params[t->esc_num_params] = t->esc_params[t->esc_num_params] * 10 + (c - '0');
            } else if (c == ';') {
                if (t->esc_num_params < 7) t->esc_num_params++;
            } else if (c == 'h' || c == 'l') {  // Set (h) or Reset (l) mode
                // Check for cursor visibility mode (25)
                if (t->esc_params[0] == 25) {
                    t->cursor_visible = (c == 'h');  // h = show, l = hide
                }
                t->esc_state = 0;
            } else {
                t->esc_state = 0;
            }
            continue;
        }

        if (c == '\x1b') {
            t->esc_state = 1;
            continue;
        }

        if (c == '\n') {
            t->cursor_x = 0;
            t->cursor_y += font_h;
        } else if (c == '\r') {
            t->cursor_x = 0;
        } else if (c == '\t') {
            t->cursor_x = (t->cursor_x + (font_w * 4)) & ~((font_w * 4) - 1);
        } else if (c == '\b') {
            if (t->cursor_x >= font_w) {
                t->cursor_x -= font_w;
                tty_draw_rect(t, t->cursor_x, t->cursor_y, font_w, font_h, t->bg_color);
            }
        } else {
            // Draw 8x8 bitmap font
            tty_draw_char(t, t->cursor_x, t->cursor_y, c, t->fg_color, t->bg_color);
            t->cursor_x += font_w;
        }

        if (t->cursor_x + font_w > t->width) {
            t->cursor_x = 0;
            t->cursor_y += font_h;
        }

        if (t->cursor_y + font_h > t->height) {
            tty_scroll(t);
        }
    }
    
    spinlock_release_irqrestore(&t->lock, flags);
}

void tty_push_key(int id, uint8_t scancode) {
    tty_t *t = tty_get(id);
    if (!t) return;
    tty_queue_push(&t->key_queue, scancode);
}

void tty_push_mouse(int id, uint8_t *packet, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return;
    for (size_t i = 0; i < len; i++) {
        tty_queue_push(&t->mouse_queue, packet[i]);
    }
}

int tty_read_key(int id, uint8_t *buf, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    return tty_queue_pop(&t->key_queue, buf, len);
}

int tty_read_mouse(int id, uint8_t *buf, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    return tty_queue_pop(&t->mouse_queue, buf, len);
}

void tty_push_char(int id, uint8_t c) {
    tty_t *t = tty_get(id);
    if (!t) return;
    tty_queue_push(&t->char_queue, c);
}

int tty_read_input(int id, char *buf, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    return tty_queue_pop(&t->char_queue, (uint8_t*)buf, len);
}

int tty_create(void) {
    uint64_t flags = spinlock_acquire_irqsave(&g_tty_global_lock);
    for (int i = 0; i < TTY_COUNT; i++) {
        if (!g_ttys[i].used) {
            g_ttys[i].used = true;
            spinlock_release_irqrestore(&g_tty_global_lock, flags);
            return i;
        }
    }
    spinlock_release_irqrestore(&g_tty_global_lock, flags);
    return -1;
}

int tty_ioctl(int id, uint64_t request, void *arg) {
    tty_t *t = tty_get(id);
    if (!t) return -1;
    
    if (request == TIOCGWINSZ) {
        struct winsize *ws = (struct winsize *)arg;
        ws->ws_row = t->height / 8;
        ws->ws_col = t->width / 8;
        ws->ws_xpixel = t->width;
        ws->ws_ypixel = t->height;
        return 0;
    }
    
    return -1;
}

int tty_destroy(int id) {
    tty_t *t = tty_get(id);
    if (!t) return -1;
    uint64_t flags = spinlock_acquire_irqsave(&g_tty_global_lock);
    t->used = false;
    spinlock_release_irqrestore(&g_tty_global_lock, flags);
    return 0;
}

void tty_write_output(int id, const char *data, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return;
    for (size_t i = 0; i < len; i++) {
        tty_queue_push(&t->out_queue, (uint8_t)data[i]);
    }
}

int tty_read_output(int id, char *buf, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    return tty_queue_pop(&t->out_queue, (uint8_t*)buf, len);
}

int tty_write_input(int id, const char *buf, size_t len) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    for (size_t i = 0; i < len; i++) {
        tty_queue_push(&t->char_queue, (uint8_t)buf[i]);
    }
    return (int)len;
}

int tty_set_foreground(int id, int pid) {
    tty_t *t = tty_get(id);
    if (!t) return -1;
    t->fg_pid = pid;
    return 0;
}

int tty_get_foreground(int id) {
    tty_t *t = tty_get(id);
    if (!t) return -1;
    return t->fg_pid;
}

void tty_blit_active(void) {
    tty_t *t = tty_get(g_active_tty);
    if (!t) return;
    
    extern void graphics_copy_buffer(uint32_t *src);
    graphics_copy_buffer(t->vfb);
}


int tty_poll(int id, struct poll_table *pt) {
    tty_t *t = tty_get(id);
    if (!t) return 0;
    
    int mask = 0;
    if (pt && pt->qproc) {
        pt->qproc(&t->char_queue.wait_queue, pt);
    }
    
    if (t->char_queue.head != t->char_queue.tail) {
        mask |= 0x0001; // POLLIN
    }
    
    mask |= 0x0004; // POLLOUT (always writable for now)
    
    return mask;
}
