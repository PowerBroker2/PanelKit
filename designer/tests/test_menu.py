from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from panelkit_studio.menu import export_menu_page, render_menu_page
from panelkit_studio.project import Project, Widget
from panelkit_studio.demo import demo_project
from panelkit_studio.menu_view import export_menu_view, members


class SDMenuExportTests(unittest.TestCase):
    def page(self):
        panel = Widget.create("Panel", "tools")
        text = Widget.create("Text", "prompt", y=0, parent="tools")
        text.properties["text"] = "Select a tool."
        link = Widget.create("Button", "nextPage", y=15, parent="tools")
        link.properties.update(text="Details", menu_link="../details.pkm")
        action = Widget.create("Button", "runTask", y=35, parent="tools")
        action.properties.update(text="Run diagnostics", menu_action="probe")
        return Project(widgets=[panel, action, text, link]), link

    def test_selected_container_export_is_ordered_and_exact(self):
        project, _ = self.page()
        with TemporaryDirectory() as folder:
            target = export_menu_page(project, Path(folder) / "index.pkm", "Tools", "tools")
            self.assertEqual(target.read_bytes(),
                             b"PKM1\nTITLE\tTools\nTEXT\tSelect a tool.\n"
                             b"LINK\tDetails\t../details.pkm\nACTION\tRun diagnostics\tprobe\n")

    def test_invalid_fields_are_rejected(self):
        project, link = self.page()
        for value in ("../details.txt", "/../details.pkm", "dir//details.pkm", "C:/details.pkm"):
            with self.subTest(value=value):
                link.properties["menu_link"] = value
                with self.assertRaises(ValueError):
                    render_menu_page(project, "Tools", "tools")
        link.properties.update(menu_link="details.pkm", menu_action="also")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            render_menu_page(project, "Tools", "tools")

    def test_full_scene_preserves_widget_types_and_instance_layouts(self):
        with TemporaryDirectory() as folder:
            outputs = export_menu_view(demo_project(), folder, "workbench", "Workbench")
            self.assertTrue(outputs)
            text = (Path(folder) / "MenuView_workbench.cpp").read_text(encoding="utf-8")
            self.assertIn("PanelMenuView::make<Scene>", text)
            for widget in demo_project().widgets:
                self.assertIn("widget_" + widget.name, text)
            self.assertIn("&widget_controls", text)
            self.assertIn("active->widget_", text)
            self.assertEqual((Path(folder) / "workbench.pkm").read_bytes(),
                             b"PKM1\nTITLE\tWorkbench\nVIEW\tworkbench\n")
            self.assertEqual(members('widgets::label.setText("widgets::label"); // widgets::label'),
                             'widget_label.setText("widgets::label"); // widgets::label')

    def test_unsupported_widgets_are_not_silently_skipped(self):
        project, _ = self.page()
        project.widgets.append(Widget.create("Slider", "level", parent="tools"))
        with self.assertRaisesRegex(ValueError, "is a Slider"):
            render_menu_page(project, "Tools", "tools")


if __name__ == "__main__":
    unittest.main()
