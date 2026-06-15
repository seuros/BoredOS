<div align="center">
  <h1>Nova Protocol Reference</h1>
  <p><em>Detailed wire protocol definitions, message formats, and event semantics.</em></p>
</div>

---

Nova uses a compact framed protocol over a UNIX domain socket. Each frame begins with a fixed-size header, followed by an optional payload.

## Nova Frame Header

All messages begin with a `NovaFrameHeader`:

```c
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t msg_type;
    uint32_t payload_size;
} __attribute__((packed)) NovaFrameHeader;
```

- `magic` must be `0x4E4F5641` (`"NOVA"`).
- `version` is currently 2.0 but for the forseeable future this will not stop Nova from rejecting frames with other version numbers.
- `flags` is reserved for future use.
- `msg_type` identifies the request or event type.
- `payload_size` is the number of bytes following the header.

## Request message types (Client → Nova)

| Constant | Meaning |
|---|---|
| `MSG_CREATE_SURFACE` | Create a new surface. |
| `MSG_RESIZE_SURFACE` | Request a new shared buffer for resize. |
| `MSG_MOVE_SURFACE` | Move an existing surface. |
| `MSG_DAMAGE` | Notify Nova that surface pixels have changed. |
| `MSG_SET_TITLE` | Update the surface title text. |
| `MSG_DESTROY_SURFACE` | Destroy a surface and free its resources. |
| `MSG_SET_INPUT_FOCUS` | (Unused in current implementation) |
| `MSG_QUERY_SCREEN` | (Unused in current implementation) |
| `MSG_SET_STATE` | Update surface state flags. |
| `MSG_SET_ICON` | Update surface icon path. |
| `MSG_QUERY_WINDOWS` | Request a window list from Nova. |
| `MSG_SET_FLAGS` | Update surface flags. |
| `MSG_QUIT` | Ask Nova compositor to exit. |
| `MSG_GET_SURFACE_GEOMETRY` | Query position and size of an existing surface. |

## Event message types (Nova → Client)

| Constant | Meaning |
|---|---|
| `EVT_KEY` | Keyboard event for the surface. |
| `EVT_POINTER` | Mouse pointer event for the surface. |
| `EVT_RESIZE_REQUEST` | Nova asks the client to resize its surface. |
| `EVT_CLOSE_REQUEST` | Client should close the surface. |
| `EVT_FOCUS_IN` | Surface gained keyboard focus. |
| `EVT_FOCUS_OUT` | Surface lost keyboard focus. |
| `EVT_THEME_UPDATE` | Theme or config reload has occurred. |
| `EVT_STATE_CHANGED` | The surface state changed. |
| `EVT_WINDOW_CREATED` | Window list report or overlay notification. |
| `EVT_WINDOW_DESTROYED` | Window was destroyed. |
| `EVT_WINDOW_TITLE_CHANGED` | Window title or icon path changed. |
| `EVT_WINDOW_LIST_END` | End of window query response. |

## Core client messages

### `MSG_CREATE_SURFACE`

Payload:

```c
struct {
    uint32_t w, h;
    uint8_t layer;
    uint32_t flags;
} __attribute__((packed));
```

Reply payload:

```c
struct {
    uint32_t surface_id;
    char shm_path[108];
} __attribute__((packed));

The `flags` field can configure custom surface traits:
- `SURFACE_FLAG_TRANSPARENT` (`0x1`): Configures the surface to support transparent pixel blending.
- `SURFACE_FLAG_NO_RESIZE` (`0x2`): Disables sizing borders and controls, preventing user resizing of the surface.
```

Nova allocates a new surface and returns a shared memory path such as `/dev/shm/nova_surf_fd%d_%u`.

### `MSG_RESIZE_SURFACE`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t w, h;
} __attribute__((packed));
```

Reply payload:

```c
struct {
    char shm_path[108];
} __attribute__((packed));
```

Nova provides a new shared memory segment for the resized content. Clients must `open()` and `mmap()` the returned path before rendering.

### `MSG_MOVE_SURFACE`

Payload:

```c
struct {
    uint32_t surface_id;
    int x, y;
} __attribute__((packed));
```

### `MSG_DAMAGE`

Payload:

```c
uint32_t surface_id;
uint32_t rect_count;
NovaRect rects[rect_count];
```

Notify Nova that one or more rectangles of the surface have changed. Nova will composite the surface contents during the next render pass.

### `MSG_SET_TITLE`

Payload:

```c
struct {
    uint32_t surface_id;
    char title[128];
} __attribute__((packed));
```

### `MSG_SET_ICON`

Payload:

```c
struct {
    uint32_t surface_id;
    char icon_path[256];
} __attribute__((packed));
```

### `MSG_SET_STATE`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t state_flags;
} __attribute__((packed));
```

Current Nova behavior uses `state_flags & 1` to denote the active/focused state.

### `MSG_SET_FLAGS`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t flags;
} __attribute__((packed));
```

Updates the surface flags (such as transparent and no-resize options) at runtime.

### `MSG_DESTROY_SURFACE`

Payload:

```c
uint32_t surface_id;
```

### `MSG_QUERY_WINDOWS`

No payload.

Nova replies with one or more `EVT_WINDOW_CREATED` events followed by a final `EVT_WINDOW_LIST_END` event.

### `MSG_GET_SURFACE_GEOMETRY`

Payload:

```c
uint32_t surface_id;
```

Reply payload:

```c
struct {
    uint32_t surface_id;
    int x, y;
    uint32_t w, h;
} __attribute__((packed));
```

Requests the current coordinates and dimensions of the specified surface.

### `MSG_QUIT`

No payload.

Instructs the Nova compositor to exit.

## Event payloads

### `EVT_KEY`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t keycode;
    uint32_t modifiers;
    uint8_t pressed;
    uint8_t text_len;
    char text[5];
    uint32_t codepoint;
} __attribute__((packed));
```

- `keycode` is a value from `NovaKeycode`.
- `modifiers` uses bit flags: `Shift` (`0x01`), `Ctrl` (`0x02`), `Alt` (`0x04`), `AltGr` (`0x08`), `CapsLock` (`0x10`).
- `pressed` is `1` for key down, `0` for key release.
- `text_len` is the number of valid bytes in `text` (maximum 4).
- `text` is a null-terminated UTF-8 translation of the key event.
- `codepoint` is the Unicode codepoint representing the key input.

### `EVT_POINTER`

Payload:

```c
struct {
    uint32_t surface_id;
    int x, y;
    uint32_t buttons;
} __attribute__((packed));
```

Coordinates are relative to the top-left of the surface content region.

### `EVT_RESIZE_REQUEST`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t w, h;
} __attribute__((packed));
```

Nova sends this when the compositor requests a client to resize its backing store.

### `EVT_CLOSE_REQUEST`, `EVT_FOCUS_IN`, `EVT_FOCUS_OUT`, `EVT_WINDOW_DESTROYED`, `EVT_WINDOW_LIST_END`

Payload:

```c
uint32_t surface_id;
```

### `EVT_WINDOW_CREATED`, `EVT_WINDOW_TITLE_CHANGED`

Payload:

```c
struct {
    uint32_t surface_id;
    char title[128];
    uint32_t state_flags;
    char icon_path[256];
} __attribute__((packed));
```

### `EVT_STATE_CHANGED`

Payload:

```c
struct {
    uint32_t surface_id;
    uint32_t state_flags;
} __attribute__((packed));
```

### `EVT_THEME_UPDATE`

No payload in the current implementation, but it is dispatched when Nova reloads theme configuration.

## Shared structures

### `NovaRect`

```c
typedef struct {
    int x, y;
    uint32_t w, h;
} NovaRect;
```

### `NovaKeycode`

Nova defines a small virtual keycode enum for keyboard events. It includes letters, digits, navigation keys, modifiers, and common control keys.

### `NovaEvent`

Clients receive events through the `NovaEvent` structure, which includes the event type, the target surface, and event-specific data.

```c
typedef struct {
    uint32_t type;
    uint32_t surface_id;
    union {
        struct { int x, y; uint32_t buttons; } pointer;
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            uint8_t pressed;
            uint8_t text_len;
            char text[5];
            uint32_t codepoint;
        } key;
        struct { uint32_t w, h; } resize;
        struct { uint32_t state_flags; } state;
        struct { char title[128]; uint32_t state_flags; char icon_path[256]; } window;
    } data;
} NovaEvent;
```

## Client helper functions

The Nova client library provides these helpers:

- `nova_connect(const char *socket_path)`
- `nova_create_surface(...)`
- `nova_resize_surface(...)`
- `nova_damage_surface(...)`
- `nova_move_surface(...)`
- `nova_set_state(...)`
- `nova_set_flags(...)`
- `nova_set_icon(...)`
- `nova_set_title(...)`
- `nova_destroy_surface(...)`
- `nova_query_windows(...)`
- `nova_quit(...)`
- `nova_poll_event(...)`
- `nova_pending_events(void)`

## Protocol behavior notes

- Nova uses a simple request/reply pattern for `MSG_CREATE_SURFACE` and `MSG_RESIZE_SURFACE`.
- Events may arrive out of order; the client library queues them internally.
- `nova_pending_events()` allows clients to poll previously received events without blocking.
- `MSG_DAMAGE` is a hint only. Nova will composite the damage on the next render pass and may merge multiple rectangles.
- Clients must not assume the shared memory path remains valid after a resize without re-opening it.

## Example: frame header creation

```c
NovaFrameHeader header;
header.magic = NOVA_MAGIC;
header.version = NOVA_VERSION;
header.flags = 0;
header.msg_type = MSG_CREATE_SURFACE;
header.payload_size = sizeof(payload);
```

## Example: receiving events

```c
if (nova_pending_events() || poll(&pfd, 1, timeout) > 0) {
    NovaEvent ev;
    while (nova_poll_event(fd, &ev) == 0) {
        switch (ev.type) {
            case EVT_CLOSE_REQUEST:
                return;
            case EVT_RESIZE_REQUEST:
                resize_surface(...);
                break;
            case EVT_KEY:
                handle_key(&ev.data.key);
                break;
        }
    }
}
```
