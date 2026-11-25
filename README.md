# PanelKit

Build embedded interfaces with widgets, nested panels, and PixelPlanner graphics. PanelKit routes touch, serial commands, and device reports through the same event signal. It keeps drags with the widget that received the press, sends typing to the focused widget, and submits a frame to TeensyTFT only when the scene changes.

## Documentation

| Guide | What it covers |
| --- | --- |
| [PanelKit Studio](designer/README.md) | Design a screen, wire callbacks, and export C++ for a new or existing Arduino sketch. |
| [Input interfaces](docs/input-interfaces.md) | Connect touch, mouse, USB Serial, and software input. Includes event routing and the CRC-8 command protocol. |
| [Modal dialogs](docs/dialogs.md) | Show messages, images, and editable controls, then handle the dialog result. |
| [SD-backed menus](docs/sd-menus.md) | Load pages on demand, handle card removal and reinsertion, and export SD menus from Studio. |
| [Serial walkthrough coverage](docs/serial-walkthrough.md) | Run the USB walkthrough and see which widgets, layouts, and input behaviors it checks. |

## Get started

Install **PanelKit**, **PixelPlanner**, and **Callback**. The display examples also use **TeensyTFT** and its ILI9341_T4 dependency. Follow TeensyTFT's wiring instructions for an SPI ILI9341 with XPT2046 touch. [TeensyTFT setup](https://github.com/PowerBroker2/TeensyTFT)

| Example | What to try |
| --- | --- |
| [PanelKit_Example](examples/PanelKit_Example/PanelKit_Example.ino) | Drag the list, select an item, and press OK to update the title. Cancel resets the title. It shows buttons, labels, a vertical layout, and bitmap fonts. |
| [PanelKit_Dashboard](examples/PanelKit_Dashboard/PanelKit_Dashboard.ino) | Drag the slider to change the simulated signal speed, or tap the switch to pause. The display combines an antialiased history plot, a level meter, a nested panel, and an atlas-based rotating needle. Send `c` in Serial Monitor to calibrate touch. |
| [PanelKit_Workbench](examples/PanelKit_Workbench/PanelKit_Workbench.ino) | Explore the Controls, Live, Images, and Calendar tabs in one sketch. |
| [PanelKit_USBInputTest](examples/PanelKit_USBInputTest/PanelKit_USBInputTest.ino) | Send mouse and keyboard commands over USB Serial and inspect the events on a Teensy 4.1. No display is needed. |
| [PanelKit_SerialWalkthrough](examples/PanelKit_SerialWalkthrough/PanelKit_SerialWalkthrough.ino) | Watch a USB-controlled pointer operate a tabbed GUI, nested inspector, scrollable table, and form on the TFT. |
| [PanelKit_SDLazyMenu](examples/PanelKit_SDLazyMenu/PanelKit_SDLazyMenu.ino) | Provision a native SD card over USB Serial and replace menu pages on demand. |

- **Controls** has the dropdown, radio choices, checkbox, stepped slider, and an inspector with a table and scroll area. Drag its title bar to an edge or corner, or resize it from the lower-right corner.
- **Live** graphs a simulated signal in real time. The switch pauses sampling, the slider changes speed, and **Clear** resets the plot history.
- **Images** compares a direct scanline image, a buffered bilinear resize, and a rotated atlas crop. Drag the slider to resize the main image and select **Fit**, **Fill**, or **Stretch** in the list.
- **Calendar** lets you navigate months and pick a date. It also shows a numeric stepper, text entry, and an on-screen keypad. Tap the text field, then type in Serial Monitor to enter letters.

## Build a scene

```cpp
#include <PanelKit.h>
#include <TeensyTFT.h>

TeensyTFT display(10, 9, 255);
Panel<320, 240, false> scene;
PanelSlider slider(180, 24, &scene);
PanelInput input;
PanelTouchInput touch(input);
bool ready = false;

void setup()
{
    ready = display.begin();
    if (!ready) return;

    input.setBounds(display.width(), display.height());
    scene.setBackground(RGB565(17, 28, 42));
    scene.activate();
    slider.setLoc(25, 50);
    slider.setValue(0.5f);
    slider.activate();
}

void loop()
{
    if (!ready) return;
    touch.pollDisplay(display);
    input.tick();
    scene.present(display);
}
```

- **Use parent-relative coordinates.** Nest a panel by passing its parent to the constructor, then use `setLoc()` relative to that parent. Children are clipped to the parent's visible rectangle. Higher `setOrder()` values draw later and get hit-tested first.
- **Choose drawing storage.** `Panel<W, H, false>` draws its background and children directly into the display frame. The original `Panel<W, H>` also provides a full-size `pixmap` for custom background art.
- **Keep widget objects alive.** Panels and layouts reference sketch-owned widgets. A link registers an object and `unlink()` removes it. `activate()` enables input, and `setVisible(false)` hides the widget and cancels input capture.
- **Present through the root.** `present()` recomposes the bounding rectangle of changed widgets and submits the frame through TeensyTFT. Changes to the root itself and structural changes may request a full repaint. After editing a component's public pixmap directly, call its `invalidate()`. Call `present(display, true)` to force a redraw.
- **Use the low-level tools when needed.** `TouchProcessor` supports immediate clicks and optional deferred clicks for double-click detection. Type events go to the most recently pressed widget. `panelStack()` positions widget references horizontally or vertically with a gap.

## Widgets

| Widget | Use |
| --- | --- |
| `PanelButton<W, H>` | Captioned button with pressed colors, optional rounded corners, alignment, and a click callback |
| `PanelLabel<W, H>` | Text that can be composited over the panel background |
| `PanelList<W, H>` | Tap-selectable rows with drag scrolling and a selection callback |
| `PanelSlider`, `PanelToggle`, `PanelProgress` | Lightweight direct-drawn controls for values and state |
| `PanelPlot<W, H, N>` | Fixed-capacity history plot with antialiased strokes |
| `PanelSprite<W, H, ATLAS_W, ATLAS_H>` | Masked atlas sprite with pivot-based rotation and scaling |
| `Component<W, H>` | Custom graphics and event callbacks |
| `PanelText` | Direct-drawn text with optional sizing to fit its content |
| `PanelCheckBox`, `PanelRadioGroup`, `PanelComboBox` | Checkable and mutually exclusive options with callbacks and configurable appearance |
| `PanelTable` | Fixed headers, configurable columns, horizontal and vertical panning, and cell or header click callbacks |
| `PanelScrollArea<W, H>` | Clipped viewport with scroll offsets, touch panning, and scrollbar indicators |
| `PanelWindow<W, H>` | Title bar, drag, resize, minimize, maximize, close, and optional modal input |
| `PanelFlex<W, H>`, `PanelGridLayout` | Horizontal and vertical weighted layouts, and retained equal-cell grids |
| `PanelTabs` | Switch between child pages |
| `PanelDialog<W, H>` | Modal panel for messages, images, form controls, and asynchronous results |
| `PanelCalendar` | Gregorian date picker with month navigation and a date callback |
| `PanelSpinBox`, `PanelTextField`, `PanelKeypad` | Bounded integer input, editable ASCII text, and reusable on-screen numeric keys |
| `PanelPicture` | Low-RAM image view with fit, fill, stretch, and anchor placement modes |

### Configure widgets

Direct-drawn controls share a `PanelTheme` with font, text height, padding, and colors. Sliders support a numeric range and optional step. Plots support a data range and a visible sample count. A table uses a callback to obtain cell text, so the sketch can own or compute its data.

`setSize()` changes logical widget dimensions. Pixmap-backed widgets can shrink within their allocated dimensions. Direct-drawn widgets and unbuffered containers can also grow. Use `PanelFlex::setHorizontalLayout()` or `setVerticalLayout()` to allocate fixed space and share remaining space by weight. Flex layouts reflow when their size or child visibility changes. `PanelWindow::fitSingleChild(true)` fits its sole child to the client area when the window resizes.

Attach a retained grid with `setGridLayout(&layout)` on a panel or window. The grid reflows when the viewport changes, and hidden items keep their cells. Keep the descriptor and its item array alive while attached. `panelGrid()` is available for one-time positioning.

```cpp
// Declare these alongside the widgets so they outlive the layout.
BaseComponent* const tiles[] = {&leftTile, &rightTile, &bottomTile};
const PanelGridLayout grid{tiles, 3, 2, 6, 6, 8};
// In setup(), after the children are registered with panel:
panel.setGridLayout(&grid);
```

Windows snap on release after a title-bar drag. The top edge maximizes, side edges fill half the parent viewport, and corners fill a quarter. Snapped windows keep their placement when their parent resizes, including inside another window. Drag a snapped window away to restore its previous size. Set `setSnapDistance(0)` to disable snapping or pass a custom threshold in pixels.

### SD-backed menus

Use `PanelMenuNavigator` for menu pages loaded on demand from a storage backend. The Teensy example reloads the entry page when an SD card is detected again. Studio can export SD-only text menus or full scenes selected by an SD page. See the [SD menu guide](docs/sd-menus.md) for the Teensy walkthrough, PKM1 file format, and Studio export.

### Modal dialogs

Show a `PanelDialog` from an event handler, add ordinary widgets as its contents, and handle the result with `onFinished()`. The normal input and display loop keeps running while the dialog is open. See the [dialog guide](docs/dialogs.md) for an example and the lifecycle and focus rules.

### Rendering and memory

Use `PanelPlot<W, H, N, false>` to draw the graph directly into the display canvas with the same history and antialiased line. The default buffered plot caches the rendered graph for reuse. `PanelButton<W, H, false>` draws its caption into the button pixmap without a second full-size buffer. The default button retains the separate text pixmap. Both modes use the button’s current size for caption alignment and support `setPadding()` for text inset.

`PanelPicture` borrows packed RGB565 source pixels and writes up to 48 pixels at a time. Use it for nearest-neighbor resizing with little scratch RAM. For bilinear or area filtering, use a buffered `Component` with PixelPlanner's `blitResized()` or `blitFromArray()`. The image converter in PixelPlanner can turn photos and graphics into RGB565 headers.

### Input devices

Connect touch, serial, mouse, and software inputs to one `PanelInput` router. Give each source its own ID so callbacks can identify where an event came from. The first source to press holds pointer capture until release or cancellation. Typing still follows focus while a pointer is held.

```cpp
PanelInput input;
PanelTouchInput touch(input);
PanelStreamInput usb(input, PanelStreamMode::COMMANDS);

// In setup(), after the display starts:
input.setBounds(display.width(), display.height());
Serial.begin(115200);

// In loop():
touch.pollDisplay(display);
usb.poll(Serial);
input.tick();
scene.present(display);
```

Use `PanelStreamMode::COMMANDS` for plain Serial Monitor commands or `PanelStreamMode::CHECKED_COMMANDS` for CRC-8/ATM-protected records. See the [input interface reference](docs/input-interfaces.md) for framing, driver contracts, event fields, and guidance on StreamSwitchboard.

### Verify input over USB Serial

Upload [PanelKit_USBInputTest](examples/PanelKit_USBInputTest/PanelKit_USBInputTest.ino) with **Board → Teensy 4.1** and **USB Type → Serial**. The sketch builds an in-memory scene and reports each event over USB. Open Serial Monitor at 115200 with **Newline** enabled. Send one line at a time.

```text
click 12 12
text Hi
key 8
down 52 12
move 83 40
up 96 55
scroll 52 12 -2
```

`RAW` lines come from the global event signal. `ENTRY` and `PAD` lines show which widget received the event. `TEXT` shows the text field’s current value, and `ERRORS` counts rejected commands.

For an automated check, close Serial Monitor and run this from the repository root. Replace `COM4` with the Teensy’s current serial port.

```sh
python -m pip install pyserial
python examples/PanelKit_USBInputTest/check_usb_input.py COM4 --verbose
```

### Walk through a GUI over USB Serial

Upload [PanelKit_SerialWalkthrough](examples/PanelKit_SerialWalkthrough/PanelKit_SerialWalkthrough.ino) to a Teensy 4.1 with **USB Type → Serial** and the TeensyTFT wiring shown above. Run the walkthrough with Serial Monitor closed.

```sh
python examples/PanelKit_SerialWalkthrough/check_gui.py COM4
```

The script moves a white-outlined pointer in visible steps and pauses after each action. It covers every widget type in the Studio catalog and checks CRC rejection, recovery after malformed frames, widget state, window geometry, and cursor redraws. Use `--pace 2` to slow the walkthrough or `--verbose` to see the device trace. The walkthrough requires checked frames. The headless USB example is available for plain commands in Serial Monitor. See the [walkthrough coverage](docs/serial-walkthrough.md) for the tested combinations.

### PanelKit Studio

Use [PanelKit Studio](designer/README.md) to arrange widgets, configure properties and callbacks, select serial input and touch settings, and export C++ into a new or existing Arduino sketch. Generated widgets and the input router are available to handwritten sketch code.

## Add a font

Export a PixelPlanner header with glyph-bridge and include it in your sketch. Keep the font object alive for as long as widgets use it.

When bitmap text is drawn smaller, PanelKit and PixelPlanner preserve source ink across each output pixel's footprint. Thin strokes stay visible instead of falling between nearest-neighbor samples. Direct-drawn controls can opt into the old sampling through `PanelTheme::fontFilter = BitmapFilter::NEAREST`.

```sh
glyph-bridge --family Inter --size 20 --export --output ./MySketch
```

```cpp
#include "font_inter_regular_20.h"

// Pixel height for labels and rows. Buttons take a scale multiplier.
heading.setFont(font_inter_regular_20::Font, 18);
menu.setFont(font_inter_regular_20::Font, 24);
btnOk.setFont(font_inter_regular_20::Font, 0.9f);
```
