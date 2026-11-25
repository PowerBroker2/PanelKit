# PanelKit Designer

Build the screen, arrange widgets, wire callbacks, and export an Arduino sketch. The Designer stores projects as editable JSON and generates standard PanelKit C++.

## Install and open

Use Python 3.10 or newer with Tk installed. On Windows, the standard Python installer includes Tk. On Linux, use your distribution's Python Tk package if Tk is unavailable.

From the PanelKit checkout, run:

```sh
python -m pip install ./designer
panelkit-designer
```

To run directly from the source directory, install Pillow and run `python -m panelkit_designer` from `designer`. Open **File → Open demo** for a project with the full widget catalog.

## Design a screen

1. Add a widget from the palette. Select a container in the object tree to place a new widget inside it.
2. Drag a widget to position it, or drag the lower-right handle to resize it. Use **Properties** for exact sizes, stacking order, themes, and widget settings, then click **Apply changes**. Children in managed layouts follow the parent layout.
3. Set one or more columns on a Panel to arrange its children in a grid. Use a Flex container for weighted rows or columns. To create tabs, add sibling page containers and choose them in the tab bar's **Pages** editor. Set **Selected** to preview a page.
4. Choose an image for a Picture, Pixmap, or Sprite. Picture uses little RAM and resizes while rendering. Pixmap provides interpolation options. Sprite supports crop, pivot, rotation, and scale. Image transparency uses a 1-bit mask, with alpha of 128 and above treated as opaque.
5. Set a glyph-bridge header and its qualified font symbol in the Screen properties, or use the included Inter bitmap font. The desktop canvas approximates layout and uses a desktop font. Compile and run the sketch to verify bitmap appearance, clipping, gesture handling, and on-device rendering.

The catalog includes panels, Flex layouts, windows, scroll areas, tabs, text, labels, buttons, checkboxes, radio groups, dropdowns, lists, sliders, progress bars, toggles, plots, tables, calendars, spin boxes, text fields, numeric keypads, pictures, sprites, and programmable pixmaps.

## Wire behavior

Open **Events & code** on a widget and use **Add widget action** to connect a callback to another widget. You can also enter C++ directly below the callback signature. To make a button clear a graph named `signal`, use its `onClick()` field:

```cpp
widgets::signal.clearSamples();
```

Screen-level code fields can add includes and declarations, run after initial configuration, or run once per frame. For example, append sensor data to a plot, bind a dynamic table model, draw into a custom pixmap, control touch calibration, or call PanelKit APIs that have no dedicated inspector field. The **Preview generated C++** menu shows the actual calls. The canvas does not execute arbitrary C++ or simulate firmware behavior.

## Export to Arduino

Install PanelKit, PixelPlanner, Callback, TeensyTFT, and ILI9341_T4 for the starter sketch. Use the matching PanelKit and PixelPlanner checkouts for transparent image support. Choose **Export to Arduino → Yes** to create a new sketch folder. Open the matching `.ino` in Arduino IDE, select Teensy 4.1, and upload after setting the display and touch pins. Starter pins default to 10, 9, 255, 8, 2. The starter uses TeensyTFT's 320 × 240 landscape display.

Choose **No** to export just the generated files into an existing sketch folder. Include `PanelKitGenerated.h`, call `panelkit_ui::configure()` once, feed touch coordinates to `panelkit_ui::touch.process(pressed, x, y)`, and call `panelkit_ui::update()` and `panelkit_ui::present(display)` in your loop. You can also drive events yourself and use any display adapter supported by PanelKit. Your sketch can access generated widgets directly as `panelkit_ui::widgets::object_id`.

Keep your handwritten code in the original sketch or separate source files. Re-export updates the generated C++ and assets without replacing an existing `.ino`. If you change the starter pin settings later, copy the new settings into your existing `.ino`.

The same generator is available without the GUI:

```sh
panelkit-designer export ControlPanel.panelkit.json ./ControlPanel
panelkit-designer export ControlPanel.panelkit.json ./ExistingSketch --integrate
```

Python code can use `Project`, `Widget.create()`, `load_project()`, `save_project()`, and `export_project()`. Keep source images and custom font headers alongside the project for portability. All runtime image arrays and the selected font header are copied into the exported sketch. A custom header with its own extra includes needs those dependencies in the Arduino project too.

