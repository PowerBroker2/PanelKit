# Lazy menus from SD

[← Back to PanelKit README](../README.md#documentation)

The [SD lazy menu walkthrough](../examples/PanelKit_SDLazyMenu/PanelKit_SDLazyMenu.ino) keeps a small shell in RAM. The shell owns the display, input router, Back and Home buttons, and cursor. The current page is read from the native SD slot and built as a child panel. Following a link validates the destination before destroying the old panel and constructing the new one in the same storage.

Only the current page’s widgets stay alive. Back stores paths and reloads a page from its file. It starts with that file’s current contents. Home reloads the index and clears navigation history. An unreadable or invalid destination leaves a working page and its history intact. On startup, an unreadable or invalid index opens a built-in recovery page. Home retries the index after the card becomes available.

There are two page types. **File-defined menus** describe text and navigation buttons entirely on the card. **Full views** are ordinary PanelKit scenes compiled into the sketch. A small SD page selects a full view by ID. Studio can generate full views using its existing widget catalog, images, bitmap fonts, layouts, and event callbacks. Each view is reconstructed when opened, including its initial form state.

## Card removal and reinsertion

The Teensy example checks a mounted card about every 350 ms and retries mounting about once a second while it is offline. Removing the card keeps the current page on screen and displays **SD offline**. Links that need a file report a read error. The current page’s controls and firmware actions keep working.

After detecting a mount, the example rereads `/PKMENU/index.pkm` and clears the old Back history automatically. It uses the index it finds on that card, so a replacement or updated tree can load on reinsertion. A readable card with no valid index shows the built-in recovery page. Home can retry the index after you add or correct it. Give the example enough time to observe removal before reinserting a card.

The USB checker exercises this logic by simulating an offline state and selecting an alternate tree on the actual SD card. That verifies retention, rereading, and rediscovery without requiring a physical card swap.

## Run the Teensy 4.1 walkthrough

Use the same display wiring as the other TeensyTFT examples. Insert an SD card the Teensy SD library can mount in the **native SD slot**. Upload the walkthrough with **USB Type → Serial**. The firmware upload uses the Teensy USB bootloader. The page files are written separately over USB Serial, with no need to remove the card.

Close Serial Monitor and run from the PanelKit checkout:

```sh
python -m pip install pyserial
python examples/PanelKit_SDLazyMenu/check_sd_menu.py COM4
```

Replace `COM4` with the Teensy’s serial port. The script checks SD and display initialization, provisions both fixture menu trees, reads each back byte for byte, and clicks through the fixture. It verifies relative links, Back, Home, actions, missing and malformed destinations, and interrupted USB transfer recovery. The sketch reports a page generation counter and the root child count. The script checks that successful transitions increment the generation, failed loads leave it unchanged, and the root contains four shell components and one active page throughout.

Use `--expect-existing` to require the index to load before any provisioning. Use `--verbose` for the board trace. You can also supply your own exported tree:

```sh
python examples/PanelKit_SDLazyMenu/check_sd_menu.py COM4 --folder path/to/menu --provision-only
```

The demo only writes under `/PKMENU`. Existing files are accepted when their bytes match. The uploader refuses to overwrite different files. Writes go through a temporary file and are read back before renaming into place. The demo does not format the card.

## PKM1 page format

A page is a printable ASCII text file with a `.pkm` extension and a maximum of 512 bytes. Each line ends in LF or CRLF, including the last. Columns are separated by literal tabs. Fields have no quoting or escapes.

| Record | Meaning |
| --- | --- |
| `PKM1` | Required first line and format version |
| `TITLE<TAB>title` | Exactly one title, 1–39 characters |
| `TEXT<TAB>message` | Up to two text rows, each 1–47 characters |
| `LINK<TAB>label<TAB>path` | A button that navigates to another page |
| `ACTION<TAB>label<TAB>name` | A button that invokes a registered sketch action |
| `VIEW<TAB>id` | Select a compiled firmware view by name. A VIEW page contains TITLE and VIEW records. |

There can be up to five buttons. Labels are 1–39 characters. Link paths and action names fit in 79 characters. Action names and path segments use ASCII letters, digits, underscores, and dashes. The final link segment ends in lowercase `.pkm`. A leading slash starts at the virtual menu root. Relative paths start in the current page’s folder. `.` and `..` are allowed as path segments. Traversal above the menu root, empty segments, and invalid link syntax are rejected.

For example, the demo resolves `../../about.pkm` from `/tools/advanced/view.pkm` to `/about.pkm`. Its storage adapter maps that virtual path to `/PKMENU/about.pkm` on the card.

## Export pages from PanelKit Studio

Create a container for a page. Add its labels, text, and buttons as direct children. On each button set **Menu link** or **Menu action**. Select the container and choose **File → Export SD menu page**. Provide the title and save the file in its intended folder. Export the entry page as `index.pkm` at the root of the tree. Studio orders visible children from top to bottom and then left to right, and rejects unsupported visible widgets. The firmware uses the PKM1 page layout.

The CLI uses the same exporter:

```sh
panelkit-studio export-menu Workshop.panelkit.json menu/tools/index.pkm --section tools --title Tools
```

For a full scene, use **File → Export full SD view** and enter the firmware view ID and page title. Studio creates a `.pkm` selector plus C++ sources and asset headers. Keep the generated C++ with the sketch and place the selector in the SD menu tree. Menu Link and Menu Action bindings on buttons use the optional navigation callback. Existing C++ event handlers, setup hooks, and frame hooks compile into the view. The shell continues to own display, input, and serial configuration.

```sh
panelkit-studio export-menu-view Workshop.panelkit.json generated --name controls --title Controls
```

Full views preserve the Studio scene within the firmware. Swapping an SD card can change the file-defined menus and which installed views are selected. Changing the full scene’s widget definitions, assets, or C++ callbacks requires regenerating and rebuilding its firmware. This keeps the card format declarative and lets the sketch control all executable behavior.

Use the regular **Export to Arduino** command for a standalone sketch.

## Use the navigator in a sketch

`PanelMenuNavigator<W, H>` takes the parent registry, a reader, an optional named-action handler, a context pointer, and viewport coordinates. Allow at least 32 pixels of width and 196 pixels of height for the built-in layout.

```cpp
bool readPage(const char* path, char* data, size_t capacity,
              size_t& length, void* context);
void runAction(const char* name, void* context);

PanelMenuNavigator<320, 208> menu(screen, readPage, runAction, nullptr, 0, 32);
```

For generated full views, include their generated headers and register them before calling `begin()`. Supply a shared arena sized and aligned for the largest view in your table. Each descriptor exposes its `size` and `alignment`. Invalid or undersized registrations leave a working page intact if that view is requested. For example:

```cpp
#include "MenuView_controls.h"

alignas(16) static unsigned char viewArena[64 * 1024];  // Adjust to the exported view sizes.
const PanelMenuView* const viewTable[] = {&panelkit_menu_controls::descriptor()};

void requestViewAction(const char* target, bool isAction)
{
    if (isAction) menu.requestAction(target);
    else menu.request(target);
}

// In setup(), before menu.begin():
menu.setViews(viewTable, 1, viewArena, sizeof(viewArena));
panelkit_menu_controls::onNavigate(requestViewAction);
// In loop(), after menu.update():
menu.updateView();
```

The reader receives a normalized virtual path and writes the file into the provided buffer. Set `length` to the exact byte count and return true only on a complete read. The native navigator is independent of the SD and serial libraries. Use the context or your own wrapper to mount and read from the storage backend.

Set the theme and call `begin()` after creating the shell. Button callbacks can call `request(path)`, `requestBack()`, or `requestHome()`. The first request queued between calls to `update()` wins. Call `update()` after input polling and before drawing. It performs synchronous storage I/O only when a page load is requested. Deferring replacement to `update()` lets the active widget’s event handler return before its page is destroyed.

`path()`, `title()`, and `generation()` expose the current page. Copy the returned strings if you need them after navigation. Back history stores up to six paths. The reader’s context and any font supplied through `setTheme()` must outlive the navigator. The action handler receives the action’s name and the same context. Copy the name if you need to keep it after the callback.

## Demo USB provisioning interface

Ordinary controls use [PanelKit checked commands](input-interfaces.md#crc-8-framing). Demo provisioning uses a separate frame on the same port. It replaces the `@` prefix with `!` and uses the same two hexadecimal CRC-8/ATM digits, colon, exact payload bytes, and LF ending.

| Admin payload | Response |
| --- | --- |
| `STATUS` | SD and display readiness |
| `START` | Reload `/index.pkm` or report the fallback |
| `PUT /path.pkm HEX` | Store an uploaded page or confirm identical existing content |
| `GET /path.pkm` | Return the stored bytes as hexadecimal |

Admin paths must be canonical, root-absolute `.pkm` paths. A failed transfer reports `ERROR` and a reason. A disconnected USB session discards any partial frame. This protocol is part of the walkthrough sketch. The native navigator’s API is the storage reader and view registry. The `MEDIA OFF`, `MEDIA DEFAULT`, and `MEDIA ALTERNATE` fixture commands exercise reconnect behavior on the same physical card.
