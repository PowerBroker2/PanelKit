# PanelKit

Build touch interfaces for Teensy displays with widgets, nested panels, and PixelPlanner graphics. PanelKit routes touch to the topmost widget, keeps drag and release events with the widget that received the press, and submits a frame to TeensyTFT only when the scene changes.

## Get started

Install **PanelKit**, **PixelPlanner**, and **Callback**. The included examples also use **TeensyTFT** and its ILI9341_T4 dependency. Follow TeensyTFT's wiring instructions for an SPI ILI9341 with XPT2046 touch. [TeensyTFT setup](https://github.com/PowerBroker2/TeensyTFT)

| Example | What to try |
| --- | --- |
| [PanelKit_Example](examples/PanelKit_Example/PanelKit_Example.ino) | Drag the list, select an item, and press OK to update the title. Cancel resets the title. It shows buttons, labels, a vertical layout, and bitmap fonts. |
| [PanelKit_Dashboard](examples/PanelKit_Dashboard/PanelKit_Dashboard.ino) | Drag the slider to change the simulated signal speed, or tap the switch to pause. The display combines an antialiased history plot, a level meter, a nested panel, and an atlas-based rotating needle. Send `c` in Serial Monitor to calibrate touch. |
| [PanelKit_Workbench](examples/PanelKit_Workbench/PanelKit_Workbench.ino) | Explore the Controls, Live, Images, and Calendar tabs in one sketch. |

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
TouchProcessor touch;
bool ready = false;

void setup()
{
    ready = display.begin();
    if (!ready) return;

    scene.setBackground(RGB565(17, 28, 42));
    scene.activate();
    slider.setLoc(25, 50);
    slider.setValue(0.5f);
    slider.activate();
}

void loop()
{
    if (!ready) return;
    const bool pressed = display.touched();
    touch.process(pressed, display.touchX(), display.touchY());
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
| `PanelFlex<W, H>`, `panelGrid()` | Weighted layout that reflows on resize, and a helper for equal grid cells |
| `PanelTabs` | Switch between child pages |
| `PanelCalendar` | Gregorian date picker with month navigation and a date callback |
| `PanelSpinBox`, `PanelTextField`, `PanelKeypad` | Bounded integer input, editable ASCII text, and reusable on-screen numeric keys |
| `PanelPicture` | Low-RAM image view with fit, fill, stretch, and anchor placement modes |

### Configure widgets

Direct-drawn controls share a `PanelTheme` with font, text height, padding, and colors. Sliders support a numeric range and optional step. Plots support a data range and a visible sample count. A table uses a callback to obtain cell text, so the sketch can own or compute its data.

`setSize()` changes logical widget dimensions. Pixmap-backed widgets can shrink within their allocated dimensions. Direct-drawn widgets and unbuffered containers can also grow. Use `PanelFlex::setLayout()` to allocate fixed space and share remaining space by weight. Keep the layout entries and choice arrays alive while their widgets use them. Call `relayout()` after changing item visibility. `PanelWindow::fitSingleChild(true)` fits its sole child to the client area when the window resizes.

Windows snap on release after a title-bar drag. The top edge maximizes, side edges fill half the parent viewport, and corners fill a quarter. Drag a snapped window away to restore its previous size. Set `setSnapDistance(0)` to disable snapping or pass a custom threshold in pixels.

### Rendering and memory

Use `PanelPlot<W, H, N, false>` to draw the graph directly into the display canvas with the same history and antialiased line. The default buffered plot caches the rendered graph for reuse. `PanelButton<W, H, false>` draws its caption into the button pixmap without a second full-size buffer. The default button retains the separate text pixmap.

`PanelPicture` borrows packed RGB565 source pixels and writes up to 48 pixels at a time. Use it for nearest-neighbor resizing with little scratch RAM. For bilinear or area filtering, use a buffered `Component` with PixelPlanner's `blitResized()` or `blitFromArray()`. The image converter in PixelPlanner can turn photos and graphics into RGB565 headers.

### PanelKit Designer

Use the [PanelKit Designer](designer/README.md) to arrange widgets on a canvas, configure properties and callbacks, and export C++ into a new or existing Arduino sketch. Generated widgets use the same PanelKit API as the examples and are available to handwritten sketch code.

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
