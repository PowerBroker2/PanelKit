# PanelKit Studio

[← Back to PanelKit README](../README.md#documentation)

Build the screen, arrange widgets, wire callbacks, and export an Arduino sketch. Studio saves your project and generates standard PanelKit C++.

## Install and open

Use Python 3.10 or newer with Tk installed. On Windows, the standard Python installer includes Tk. On Linux, use your distribution's Python Tk package if Tk is unavailable.

From the PanelKit checkout, run:

```sh
python -m pip install ./designer
panelkit-studio
```

If you upgraded from PanelKit Designer, use the new `panelkit-studio` command. You can remove the old launcher with `python -m pip uninstall panelkit-designer`. Existing `.panelkit.json` projects and generated sketch folders still work.

The typeface browser uses the updated Glyph Bridge API. Install the local Glyph Bridge checkout alongside Studio with `python -m pip install -e path/to/glyph-bridge -e ./designer`. The bundled Inter font, bitmap header import, and Arduino export work with the base Studio installation.

To launch from source after installing dependencies, run `python -m panelkit_studio` from `designer`. Open **File → Open demo** to explore the complete widget catalog.

## Design a screen

1. Add a widget from the palette. Select a container in the object tree to place a new widget inside it.
2. Drag a widget to position it, or drag the lower-right handle to resize it. Use **Properties** for exact sizes, stacking order, themes, and widget settings, then click **Apply changes**. Children in managed layouts follow the parent layout.
3. Set the grid columns, gaps, and padding on a Panel or Window. Use a Flex container for a horizontal or vertical layout with fixed space and stretch weights. Attach tab pages in the **Pages** editor, then use **Selected** to preview a page. Tables, layouts, and color themes use their own structured property editors.
4. Choose an image for a Picture, Pixmap, or Sprite. Picture uses little RAM and resizes while rendering. Pixmap provides interpolation options. Sprite supports crop, pivot, rotation, and scale. Image transparency uses a 1-bit mask, with alpha of 128 and above treated as opaque.
5. Select **Screen → Choose typeface** to browse installed fonts or Google Fonts, or open a TTF or OTF file. Studio shows the available styles and a bitmap sample, then embeds the Glyph Bridge output in the project. The bundled Inter font and bitmap header import are also available. Buttons preview the actual bitmap and honor horizontal alignment, vertical alignment, padding, and corner radius. Use the device to verify the remaining widget rendering and gestures.

Undo with **Ctrl+Z** and redo with **Ctrl+Y**. Use **Edit → Clear history** to discard the current history. Closing or switching projects clears it, while saving keeps it for the current session.

The catalog includes panels, Flex layouts, windows, scroll areas, tabs, text, labels, buttons, checkboxes, radio groups, dropdowns, lists, sliders, progress bars, toggles, plots, tables, calendars, spin boxes, text fields, numeric keypads, pictures, sprites, and programmable pixmaps.

## Export a page for SD navigation

Set **Menu link** or **Menu action** on each button, select the page container, and choose **File → Export SD menu page**. Studio writes a file-defined PKM1 page from its visible direct Text, Label, and Button children. For a full widget tree, use **File → Export full SD view**. It writes a PKM1 selector and firmware source that preserves the Studio scene, including assets, layouts, and callbacks. SD files select installed views. Changes to the scene itself require a firmware rebuild. See the [SD menu guide](../docs/sd-menus.md) for the format, CLI, and USB provisioning.

## Input devices

Select **Screen → Input devices** to enable touch, configure click timing, and choose a serial port, baud rate, and per-frame byte budget. Touch and serial can stay enabled together. The first device to press owns the pointer until it releases, and keyboard input still follows focus. **Text** forwards bytes to the focused widget. **Commands** accepts lines such as `click 20 40`, `text Hello`, and `scroll 20 40 1`. Set Serial Monitor to send a newline. **Checked commands** requires an `@HH:` CRC-8/ATM prefix on every line and is intended for software senders. See the [PanelKit input interface reference](../docs/input-interfaces.md) for the full command grammar, event fields, and driver requirements.

Use **Events & code → onInput** on any widget to handle coordinates, source IDs, typing, press, release, drag, and wheel events. For mouse, HID, sensors, or software commands, feed reports to `panelkit_ui::input` from sketch code. Hardware libraries still supply the original device reports.

## Wire behavior

Open **Events & code** on a widget and use **Add widget action** to connect a callback to another widget. You can also enter C++ directly below the callback signature. To make a button clear a graph named `signal`, use its `onClick()` field:

```cpp
widgets::signal.clearSamples();
```

Screen-level code fields can add includes and declarations, run after initial configuration, or run once per frame. For example, append sensor data to a plot, bind a dynamic table model, draw into a custom pixmap, control touch calibration, or call PanelKit APIs that have no dedicated inspector field. The **Preview generated C++** menu shows the actual calls. The canvas does not execute arbitrary C++ or simulate firmware behavior.

## Export to Arduino

Install PanelKit, PixelPlanner, Callback, TeensyTFT, and ILI9341_T4 for the starter sketch. Use the matching PanelKit and PixelPlanner checkouts for transparent image support. Choose **Export to Arduino → Yes** to create a new sketch folder. Open the matching `.ino` in Arduino IDE, select Teensy 4.1, and upload after setting the display and touch pins. Starter pins default to 10, 9, 255, 8, 2. The starter uses TeensyTFT's 320 × 240 landscape display.

Choose **No** to export just the generated files into an existing sketch. Include `PanelKitGenerated.h` and call these hooks from your sketch.

| Generated hook | When to call it |
| --- | --- |
| `panelkit_ui::configure()` | Once after display initialization. Sets input bounds and configures the widgets. |
| `panelkit_ui::beginInputs()` | In setup if Studio should initialize the selected serial port. An existing sketch can initialize that port itself. |
| `panelkit_ui::pollTouch(display)` | In the loop for a TeensyTFT-compatible display. With touch enabled, a custom driver can use `panelkit_ui::touchInput.poll(driver)` instead. |
| `panelkit_ui::pollInputs()` | In the loop to consume the selected serial port up to the configured byte budget. |
| `panelkit_ui::update()` | After polling inputs. Services deferred clicks and runs the configured frame code. |
| `panelkit_ui::present(display)` | After updating the scene. Submits any changes to the display. |

The generated starter already calls these hooks. Disabling touch or setting the serial port to **Off** makes its poll hook a no-op. Custom sources can call `panelkit_ui::input`. For an older sketch that polls the legacy `panelkit_ui::touch` processor directly, migrate that polling to the router before combining pointer sources. Your sketch can access generated widgets as `panelkit_ui::widgets::object_id`.

Keep your handwritten code in the original sketch or separate source files. Re-export updates the generated C++ and assets without replacing an existing `.ino`. If you change the starter pin settings later, copy the new settings into your existing `.ino`.

The same generator is available without the GUI:

```sh
panelkit-studio export ControlPanel.panelkit.json ./ControlPanel
panelkit-studio export ControlPanel.panelkit.json ./ExistingSketch --integrate
```

Python code can use `Project`, `Widget.create()`, `load_project()`, `save_project()`, and `export_project()`. Keep source images and custom font headers alongside the project for portability. All runtime image arrays and the selected font header are copied into the exported sketch. A custom header with its own extra includes needs those dependencies in the Arduino project too.

