<div align="center">
  <h1>Nova Toolkit (NTK) Developer Guide</h1>
  <p><em>The unified widget toolkit and SDK for building premium GUI applications on BoredOS.</em></p>
</div>

---

The **Nova Toolkit (NTK)** is the native user interface framework for BoredOS. It is written in C and wraps the low-level Nova wire protocol and shared-memory framebuffer commands in a clean, hierarchical widget system. 

Instead of managing UNIX sockets, mapping shared memory paths manually, or listening for raw keyboard scancodes, developers use NTK to build structured windows, manage automatic layouts, attach signals, and draw on a managed canvas.

---

## 1. Core Application Lifecycle

Every NTK application centers around a single global app instance and an event dispatch loop.

### Lifecycle Functions

- `NtkApp* ntk_app_new(void)`  
  Initializes standard library graphics subsystems and establishes a connection to `/tmp/nova.sock`. Returns `NULL` if connection fails.
- `int ntk_app_run(NtkApp *app)`  
  Enters the application poll event loop. This blocks execution, drains socket frames, updates widgets, and routes inputs. Returns when `ntk_app_quit` is called.
- `void ntk_app_quit(NtkApp *app)`  
  Instructs the active event loop to terminate, returning from `ntk_app_run()`.
- `void ntk_app_destroy(NtkApp *app)`  
  Closes socket sockets, destroys active windows, maps out shared memory buffers, and cleans up app structures.

### Standard Template

```c
#include "ntk.h"

int main(void) {
    NtkApp *app = ntk_app_new();
    if (!app) return 1;

    NtkWidget *win = ntk_window_new("App Title", 400, 300);
    if (!win) {
        ntk_app_destroy(app);
        return 1;
    }

    // Initialize layout and widgets here...

    ntk_widget_show(win);
    int rc = ntk_app_run(app);

    ntk_app_destroy(app);
    return rc;
}
```

---

## 2. Window and Dialog Management

Windows represent compositor-level surfaces. NTK handles window chrome, movement, minimize, close triggers, and layers.

### Window Creators

- `NtkWidget* ntk_window_new(const char *title, int width, int height)`  
  Creates a standard decorated window mapped to Nova layer `2`.
- `NtkWidget* ntk_window_new_dialog(NtkWidget *parent, const char *title, int width, int height)`  
  Creates a modal or transient dialog window parented to another window.
- `NtkWidget* ntk_window_new_popup(NtkWidget *parent, int x, int y, int width, int height)`  
  Creates a frameless overlay popup (layer `4`) with `SURFACE_FLAG_NO_RESIZE` enabled (ideal for menus or context panels).

### Window Properties and Control

- `void ntk_window_set_resizable(NtkWidget *w, bool resizable)` — Toggle resize borders.
- `void ntk_window_set_content(NtkWidget *w, NtkWidget *content)` — Attach a child container/widget as the root body.
- `void ntk_window_set_menubar(NtkWidget *w, NtkWidget *menubar)` — Attach a standard top-aligned menu bar.
- `void ntk_window_set_toolbar(NtkWidget *w, NtkWidget *toolbar)` — Attach a top toolbar.
- `void ntk_window_set_statusbar(NtkWidget *w, NtkWidget *statusbar)` — Attach a bottom status bar.
- `void ntk_window_close(NtkWidget *w)` — Close and destroy the window.

### Built-in Dialog Modals

NTK provides three lightweight dialog helpers that block inputs on parent windows:

```c
// 1. Info / warning Message Box
ntk_dialog_message(win, "Information", "Task successfully saved!", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);

// 2. Boolean Question Box
bool yes = ntk_dialog_question(win, "Confirm", "Do you wish to delete this item?");

// 3. Text Input Dialog (caller is responsible for free()ing returned string)
char *text = ntk_dialog_get_text(win, "Username Input", "Enter name:", "admin");
```

---

## 3. Container Layouts

Rather than hardcoding absolute widget coordinates, NTK recommends using layout boxes and grids to scale interfaces responsively.

### Box Layout (`NtkBox`)

`NtkBox` arranges child widgets linearly, either vertically or horizontally.

- `NtkWidget* ntk_box_new(NtkOrientation orientation, NtkWidget *parent)`  
  Creates a new layout box. Orientation can be `NTK_HORIZONTAL` or `NTK_VERTICAL`.
- `void ntk_box_set_spacing(NtkWidget *box, int spacing)`  
  Defines space between packed widgets (in pixels).
- `void ntk_box_pack_start(NtkWidget *box, NtkWidget *child, bool expand, bool fill, int padding)`  
  Appends a widget to the container.
  - `expand`: If `true`, the child is given extra space when the window grows.
  - `fill`: If `true`, the child stretches to occupy its allotted cell bounds.

### Grid Layout (`NtkGrid`)

`NtkGrid` arranges widgets in standard table rows and columns.

- `NtkWidget* ntk_grid_new(NtkWidget *parent)`
- `void ntk_grid_attach(NtkWidget *grid, NtkWidget *child, int column, int row, int colspan, int rowspan)`

### Splitter (`NtkSplitter`)

Provides a draggable bar separating two user sections:

- `NtkWidget* ntk_splitter_new(NtkOrientation orientation, NtkWidget *parent)`
- `void ntk_splitter_set_widgets(NtkWidget *splitter, NtkWidget *w1, NtkWidget *w2)`

### Tabs (`NtkTabWidget`)

Manages stacked tabbed workspaces:

- `NtkWidget* ntk_tab_widget_new(NtkWidget *parent)`
- `void ntk_tab_widget_add_page(NtkWidget *tabs, NtkWidget *child, const char *label)`

---

## 4. Standard Widgets Reference

| Widget Class Name | Creation API Function | Purpose |
|---|---|---|
| **Label** | `ntk_label_new(text, parent)` | Simple read-only text container. Supports `ntk_label_set_alignment`. |
| **Button** | `ntk_button_new(text, parent)` | Trigger actions via clicking. Emits `"clicked"` signal. |
| **CheckBox** | `ntk_checkbox_new(text, parent)` | Stateful toggle representing options. |
| **RadioButton** | `ntk_radio_button_new(text, parent)` | Mutual exclusion options. Grouped with `NtkRadioGroup`. |
| **ProgressBar** | `ntk_progress_bar_new(parent)` | Visual fill status bar. Set status with `ntk_progress_bar_set_value(0.0-1.0)`. |
| **TextEntry** | `ntk_text_entry_new(parent)` | Single-line editable input. Supports placeholders. |
| **TextArea** | `ntk_text_area_new(parent)` | Scrollable multi-line editable text editor. |
| **SpinBox** | `ntk_spin_box_new(parent)` | Numeric integer step selectors with range bounds. |
| **Slider** | `ntk_slider_new(orientation, parent)` | Linear visual scale tuner. |
| **ComboBox** | `ntk_combo_box_new(parent)` | Dropdown select items lists. |
| **ListBox** | `ntk_list_box_new(parent)` | Linear scrolling text items list. |
| **TreeView** | `ntk_tree_view_new(parent)` | Collapsible hierarchical tree node viewer. |
| **TableView** | `ntk_table_view_new(parent)` | Formatted matrix cells view. Supports headers and widths. |
| **Canvas** | `ntk_canvas_new(parent)` | Managed drawing surface. Triggers paint callbacks. |

---

## 5. Signals and Event Handlers

NTK supports a lightweight, string-based signals system. Standard interactive widgets emit predefined events when users click, type, or interact.

### Signal Binding API

- `void ntk_widget_connect(NtkWidget *w, const char *signal, NtkCallback cb, void *userdata)`  
  Registers a callback function to run when the widget emits the named signal.
- `void ntk_widget_disconnect(NtkWidget *w, const char *signal, NtkCallback cb)`  
  Removes a previously registered callback.
- `void ntk_widget_emit(NtkWidget *w, const char *signal, void *event_data)`  
  Manually triggers a signal.

### Example: Handling button click

```c
static void on_save_clicked(NtkWidget *w, void *userdata) {
    NtkWidget *win = (NtkWidget *)userdata;
    ntk_dialog_message(win, "Notification", "File Saved!", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
}

// Inside setup:
NtkWidget *btn = ntk_button_new("Save", vbox);
ntk_widget_connect(btn, "clicked", on_save_clicked, win);
```

---

## 6. Custom Painting API

If your application demands custom graphics, charts, or games, use the `NtkCanvas` widget combined with the `NtkPainter` context.

### Using NtkCanvas

Initialize a Canvas widget and assign a draw callback:

```c
static void on_draw(NtkCanvas *canvas, NtkPainter *painter, void *userdata) {
    // 1. Wipe the background
    ntk_painter_clear(painter, ntk_color_from_rgb(240, 240, 240));

    // 2. Set colors and draw lines, rectangles, ellipses, or text
    ntk_painter_set_color(painter, ntk_color_from_rgb(0, 102, 204));
    ntk_painter_fill_rect(painter, NTK_RECT(10, 10, 80, 50));

    ntk_painter_set_color(painter, ntk_color_from_rgb(204, 0, 0));
    ntk_painter_draw_rounded_rect(painter, NTK_RECT(110, 10, 80, 50), 8);

    ntk_painter_set_color(painter, ntk_color_from_rgb(0, 0, 0));
    ntk_painter_draw_text(painter, "Nova Drawing API Demo", 10, 80);
}

// In main setup:
NtkWidget *canvas = ntk_canvas_new(vbox);
ntk_canvas_set_draw_callback(canvas, on_draw, NULL);
```

### Painter Context Functions

- `void ntk_painter_clear(NtkPainter *p, NtkColor c)` — Wipe the context with a flat color.
- `void ntk_painter_set_color(NtkPainter *p, NtkColor c)` — Update current fill/stroke color.
- `void ntk_painter_fill_rect(NtkPainter *p, NtkRect r)` — Draw a solid rectangle.
- `void ntk_painter_draw_rounded_rect(NtkPainter *p, NtkRect r, int radius)` — Draw an outlined rounded rectangle.
- `void ntk_painter_fill_ellipse(NtkPainter *p, NtkRect r)` — Draw a solid ellipse boundary.
- `void ntk_painter_draw_line(NtkPainter *p, int x1, int y1, int x2, int y2)` — Draw a line between coordinates.
- `void ntk_painter_draw_text(NtkPainter *p, const char *text, int x, int y)` — Render anti-aliased font text.
- `NtkGradient* ntk_gradient_new_linear(NtkPoint start, NtkPoint end)` — Create a linear gradient builder.
- `void ntk_gradient_add_stop(NtkGradient *lg, float offset, NtkColor c)` — Add color bands to a gradient.
- `void ntk_painter_draw_gradient(NtkPainter *p, NtkGradient *g, NtkRect r)` — Draw a gradient within region bounds.

---

## 7. App Utilities

### Timers

Timers let applications schedule periodic actions without locking up the event loop:

- `uint32_t ntk_app_set_timer(uint32_t interval_ms, NtkTimerCallback cb, void *userdata)`  
  Starts a recurring timer and returns a unique `timer_id`.
- `void ntk_app_kill_timer(uint32_t timer_id)`  
  Halts the timer matching the ID.

### System Clipboard

Manage cross-application copy/paste buffers:

- `void ntk_app_set_clipboard(const char *text)` — Copy text to system clipboard.
- `char* ntk_app_get_clipboard(void)` — Retrieve text from clipboard (caller must `free()` the returned string).
