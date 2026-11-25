import tempfile
import unittest
from pathlib import Path

from PIL import Image

from panelkit_studio import Project, Widget, export_project, load_project, save_project
from panelkit_studio.catalog import WIDGETS
from panelkit_studio.demo import demo_project
from panelkit_studio.export import ASSETS, HEADER, SOURCE, cpp


class StudioExportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_all_widgets_round_trip_and_export(self):
        original = demo_project()
        saved = save_project(original, self.root / "Workbench.panelkit.json")
        project = load_project(saved)
        self.assertEqual(original, project)
        self.assertEqual({widget.kind for widget in project.widgets}, set(WIDGETS))
        sketch = self.root / "Workbench"
        result = export_project(project, sketch)
        self.assertIn(sketch / "Workbench.ino", result)
        header = (sketch / HEADER).read_text()
        source = (sketch / SOURCE).read_text()
        for widget in project.widgets:
            self.assertIn(f" {widget.name};", header)
        for operation in ("setPages", "setLayout", "setPose", "setModel",
                          "onCellClick", "setDate", "onKey"):
            self.assertIn(f".{operation}(", source)

    def test_integration_and_reexport_keep_user_code(self):
        sketch = self.root / "MySketch"
        sketch.mkdir()
        source = sketch / "MySketch.ino"
        source.write_text("void setup() {}\nvoid loop() {}\n")
        custom = sketch / "Extra.cpp"
        custom.write_text("int custom = 5;\n")
        project = Project()
        outputs = export_project(project, sketch, create_sketch=False)
        self.assertNotIn(source, outputs)
        project.widgets.append(Widget.create("Button", "confirm"))
        export_project(project, sketch)
        self.assertIn("confirm;", (sketch / HEADER).read_text())
        self.assertEqual("void setup() {}\nvoid loop() {}\n", source.read_text())
        self.assertEqual("int custom = 5;\n", custom.read_text())

    def test_refuses_to_replace_a_user_header(self):
        sketch = self.root / "Existing"
        sketch.mkdir()
        user_header = sketch / HEADER
        user_header.write_text("// Handwritten")
        with self.assertRaisesRegex(ValueError, "replace a user file"):
            export_project(Project(), sketch, create_sketch=False)
        self.assertEqual(list(sketch.iterdir()), [user_header])

    def test_shared_image_mask_sprite_region_and_grid(self):
        pixels = Image.new("RGBA", (2, 3), (180, 20, 30, 255))
        pixels.putpixel((1, 1), (0, 255, 0, 0))
        pixels.save(self.root / "photo.png")
        project = Project(path=self.root / "layout.panelkit.json")
        grid = Widget.create("Panel", "gallery")
        grid.properties["grid_columns"] = 2
        project.widgets.append(grid)
        for kind in ("Picture", "Pixmap", "Sprite"):
            widget = Widget.create(kind, kind.lower(), parent=grid.name)
            widget.properties["image"] = "photo.png"
            project.widgets.append(widget)
        target = self.root / "Media"
        export_project(project, target)
        arrays = (target / ASSETS).read_text()
        self.assertEqual(arrays.count("static const uint16_t"), 1)
        self.assertIn("pixels_0_mask[] PROGMEM", arrays)
        source = (target / SOURCE).read_text()
        self.assertIn("setSource(pixels_0, 2, 3, true, pixels_0_mask)", source)
        self.assertIn(".setGridLayout(", source)
        self.assertIn("blitFromArray(pixels_0", source)
        widget.properties["region"] = [0, 0, 3, 3]
        with self.assertRaisesRegex(ValueError, "sprite region"):
            export_project(project, target)

    def test_reexport_migrates_legacy_headers_and_keeps_typeface(self):
        sketch = self.root / "Legacy"
        project = Project()
        path = project.asset(self.root/"panelkit_designer"/"assets"/"workbench.ppm")
        self.assertTrue(path.is_file())
        export_project(project, sketch)
        header = sketch / HEADER
        previous = header.read_text(encoding="utf-8").replace("PanelKit Studio", "PanelKit Designer")
        header.write_text(previous,encoding="utf-8")
        project.font_embedded = {
            "label": "Custom face", "symbol": "custom::Font",
            "header": "#pragma once\n// Embedded bitmap\n", "license": "Example font license\n",
        }
        saved = save_project(project,self.root/"Legacy.panelkit.json")
        export_project(load_project(saved),sketch)
        self.assertIn("PanelKit Studio",header.read_text(encoding="utf-8"))
        self.assertIn("&custom::Font",(sketch/SOURCE).read_text(encoding="utf-8"))
        self.assertIn("Embedded bitmap",(sketch/"PanelKitFont.h").read_text(encoding="utf-8"))
        self.assertIn("Example font license",(sketch/"PanelKitTypeface-License.txt").read_text(encoding="utf-8"))

    def test_input_settings_and_typed_slot_export(self):
        project = Project(touch_enabled=False, serial_port="Serial2", serial_format="Commands",
                          serial_baud=57600, serial_budget=24)
        button = Widget.create("Button", "confirm")
        button.events["onInput"] = "if (event.type == EventType::PRESS) widgets::confirm.invalidate();"
        project.widgets.append(button)
        saved = save_project(project, self.root / "Input.panelkit.json")
        project = load_project(saved)
        target = self.root / "InputDemo"
        export_project(project, target)
        source = (target / SOURCE).read_text()
        starter = (target / "InputDemo.ino").read_text()
        for fragment in ("Serial2.begin(57600UL)", "serialInput.poll(Serial2, 24)",
                         "PanelStreamMode::COMMANDS", "onInput(", "const Event& event"):
            self.assertIn(fragment, source)
        self.assertNotIn("PanelTouchInput", (target / HEADER).read_text())
        self.assertIn("panelkit_ui::pollTouch(display)", starter)
        self.assertIn("panelkit_ui::pollInputs()", starter)
        project.serial_format = "Checked commands"
        export_project(project, target)
        self.assertIn("PanelStreamMode::CHECKED_COMMANDS", (target / SOURCE).read_text())
        project.serial_port = "Off"
        project.touch_enabled = True
        export_project(project, target)
        self.assertEqual(starter, (target / "InputDemo.ino").read_text())
        self.assertIn("touchInput.pollDisplay(display)", (target / HEADER).read_text())
        self.assertIn("PanelTouchInput touchInput(input);", (target / SOURCE).read_text())
        self.assertNotIn("PanelStreamInput", (target / HEADER).read_text())
        self.assertNotIn("Serial2", (target / SOURCE).read_text())

    def test_invalid_references_and_cpp_escaping(self):
        project = Project(widgets=[Widget.create("Panel", "loop", parent="loop")])
        with self.assertRaisesRegex(ValueError, "cycle"):
            export_project(project, self.root / "Demo")
        self.assertEqual(cpp('"é\n'), r'"\"\303\251\012"')


if __name__ == "__main__":
    unittest.main()

