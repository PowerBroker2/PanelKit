"""A project that exercises the full widget catalog."""

from pathlib import Path

from .project import Project, Widget


def demo_project():
    project = Project()

    def add(kind, name, x, y, width, height, parent="", **properties):
        widget = Widget.create(kind, name, x, y, parent)
        widget.width, widget.height = width, height
        widget.properties.update(properties)
        project.widgets.append(widget)
        return widget

    add("Tabs", "tabs", 6, 5, 308, 26,
        pages=[["Controls", "controls"], ["Live", "live"], ["Media", "media"], ["Date", "dates"]])
    for name in ("controls", "live", "media", "dates"):
        add("Panel", name, 0, 36, 320, 204)
    add("Flex", "controlsLayout", 6, 8, 121, 183, "controls",
        gap=4, padding=3, layout=[["summary", 19, 0], ["mode", 24, 0], ["units", 55, 0],
                                  ["show", 24, 0], ["gain", 22, 0]])
    add("Text", "summary", 0, 0, 112, 19, "controlsLayout", text="Control panel")
    mode = add("Combo Box", "mode", 0, 0, 112, 24, "controlsLayout",
               items=["Overview", "Diagnostics", "Settings"])
    mode.events["onChange"] = "widgets::summary.setText(value == 0 ? \"Overview\" : \"Mode changed\");"
    add("Radio Group", "units", 0, 0, 112, 55, "controlsLayout", items=["Volts", "Percent"])
    show = add("Check Box", "show", 0, 0, 112, 24, "controlsLayout", text="Show window", checked=True)
    show.events["onChange"] = "widgets::inspector.setVisible(checked);"
    add("Slider", "gain", 0, 0, 112, 22, "controlsLayout", range=[0.0,100.0], step=5.0, value=45.0)
    window = add("Window", "inspector", 135, 8, 177, 182, "controls", fit_single_child=True)
    window.order = 1
    window.events["onClose"] = "widgets::show.setChecked(false);"
    add("Flex", "client", 0, 0, 177, 158, "inspector", gap=2, padding=3,
        layout=[["table", 88, 0], ["scrollArea", 45, 1]])
    table = add("Table", "table", 0, 0, 160, 89, "client",
        columns=[["Name", 83], ["Reading", 85]],
        rows=[["Input A", "1.25 V"], ["Input B", "2.70 V"], ["Input C", "3.01 V"]])
    table.events["onCellClick"] = '(void)column;\nwidgets::summary.setText(row < 0 ? "Column header" : "Reading selected");'
    add("Scroll Area", "scrollArea", 0, 0, 160, 45, "client", content_size=[168,96])
    add("Label", "hint", 5, 0, 157, 20, "scrollArea", text="Drag the title bar")
    add("Text", "hint2", 5, 47, 157, 20, "scrollArea", text="Scroll to read more")

    add("Label", "telemetry", 8, 4, 208, 20, "live", text="Live telemetry")
    add("Plot", "signal", 8, 30, 304, 110, "live", demo_signal=True)
    add("Progress", "meter", 8, 148, 210, 12, "live")
    live = add("Toggle", "running", 248, 143, 57, 23, "live", checked=True)
    live.events["onChange"] = "(void)checked;"
    button = add("Button", "resetPlot", 8, 170, 86, 27, "live", text="Clear")
    button.events["onClick"] = "widgets::signal.clearSamples();"
    project.frame_cpp = ("widgets::meter.setValue(widgets::running.isChecked()\n"
                         "    ? (sinf(millis() * 0.005f) + 1.0f) * 0.5f : 0.0f);")

    image = str(Path(__file__).parent / "assets" / "workbench.ppm")
    add("Picture", "picture", 7, 6, 169, 114, "media", image=image, mode="FIT")
    add("Sprite", "sprite", 202, 9, 104, 104, "media", image=image,
        pose={"x":52, "y":52, "scaleX":3, "scaleY":3, "angle":19})
    add("Pixmap", "imageBuffer", 198, 123, 103, 64, "media", image=image)
    items = add("List", "fitModes", 7, 124, 169, 71, "media", items=["Fit", "Fill", "Stretch"])
    items.events["onChange"] = ("const BlitMode modes[] = {BlitMode::FIT, BlitMode::FILL, BlitMode::STRETCH};\n"
                                "if (value >= 0 && value < 3) widgets::picture.setMode(modes[value]);")

    add("Calendar", "calendar", 7, 6, 192, 166, "dates", date=[2026,1,1])
    add("Spin Box", "quantity", 211, 12, 101, 26, "dates", range=[0,100,5], value=20)
    add("Text Field", "entry", 211, 50, 101, 27, "dates", text="")
    keypad = add("Keypad", "keypad", 211, 85, 101, 104, "dates")
    keypad.events["onKey"] = ("Event input{};\ninput.type = EventType::TYPE;\n"
                              "input.character = key;\nwidgets::entry.handleEvent(input);")
    return project

