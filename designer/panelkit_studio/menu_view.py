"""Build the full Studio scene as a firmware-side, lazily constructed SD view."""

from importlib.resources import files
from pathlib import Path
import re

from .export import Exporter, GENERATED, block, cpp
from .menu import field, link_path, NAME
from .project import atomic_text, values

TOKENS = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\b(?:panelkit_ui::)?widgets::(?P<widget>[A-Za-z_]\w*)'
)


def members(code, prefix=""):
    return TOKENS.sub(lambda match: prefix + "widget_" + match["widget"]
                      if match["widget"] else match[0], code)


def export_menu_view(project, target, name, title):
    """Write a PKM1 view selector with the C++ sources that implement that view."""
    if not NAME.fullmatch(name) or "-" in name or not name[0].isalpha() or len(name) > 39:
        raise ValueError("Use a view ID starting with a letter, followed by letters, digits or underscores.")
    title = field(title, 39, "Page title")
    builder = Exporter(project)
    builder.add_widgets()
    namespace = f"panelkit_menu_{name}"
    base = f"MenuView_{name}"
    hooks = list(builder.callbacks)
    wiring = []
    for index, widget in enumerate(builder.widgets):
        if widget.kind != "Button":
            continue
        data = values(widget)
        link, action = data["menu_link"], data["menu_action"]
        if not (link or action):
            continue
        if link and action:
            raise ValueError(f"{widget.name} needs exactly one Menu link or Menu action.")
        if link:
            link_path(link)
        elif not NAME.fullmatch(field(action, 79, "Menu action")):
            raise ValueError(f"{widget.name} has an invalid action name.")
        original = f"    item_{index}_onClick();\n" if "onClick" in widget.events else ""
        hooks.append(f"static void viewLink_{index}()\n{{\n{original}"
                     f"    if (navigate) navigate({cpp(link or action)}, {cpp(bool(action))});\n}}")
        wiring.append(f"widget_{widget.name}.onClick(viewLink_{index});")

    font = project.font_embedded.get("header")
    if not font and project.font_header:
        font = project.asset(project.font_header).read_text(encoding="utf-8-sig")
    if not font:
        font = files("panelkit_studio").joinpath("assets", "font_inter_regular_16.h").read_text(encoding="utf-8")
    license_text = project.font_embedded.get("license")
    if not project.font_embedded and not project.font_header:
        license_text = files("panelkit_studio").joinpath("assets", "OFL-Inter.txt").read_text(encoding="utf-8")

    atlases = [line.removeprefix("static ") for line in builder.data if line.startswith("static Pixmap<")]
    constants = [line for line in builder.data if not line.startswith("static Pixmap<")]
    declarations = [f"{kind} widget_{identifier};" for kind, identifier, _ in builder.widget_specs]
    initialization = [f"widget_{identifier}({members(args)})"
                      for _, identifier, args in builder.widget_specs]
    persistent_layouts = [members(line.removeprefix("static ")) for line in builder.instance_data]
    setup = ["screen.setLoc(viewport.x, viewport.y);",
             "screen.setSize(viewport.width, viewport.height);",
             *[members(line) for line in builder.setup + builder.tabs + builder.windows + builder.layout],
             *wiring, *members(project.configure_cpp).splitlines()]
    # Demo signal timers belong to the scene so a reconstructed view starts cleanly.
    frame_members = [line.removeprefix("static ") for line in builder.frame if line.startswith("static ")]
    frames = [members(line) for line in builder.frame if not line.startswith("static ")]
    frames += members(project.frame_cpp).splitlines()
    class_content = [f"{builder.root_type} screen;", *atlases, *declarations, *persistent_layouts,
                     *frame_members, "Scene(ComponentRegistry& parent, PixmapRect viewport);",
                     "~Scene() { active = nullptr; }", "void update()", "{", block(frames), "}"]
    header = "\n".join([GENERATED.rstrip(), "#pragma once", "#include <PanelKit.h>",
                        f"namespace {namespace} {{", "const PanelMenuView& descriptor();",
                        "void onNavigate(void (*callback)(const char* target, bool isAction));", "}", ""])
    source = "\n".join([GENERATED.rstrip(), f'#include "{base}.h"', f'#include "{base}_font.h"',
                        f'#include "{base}_images.h"', project.includes,
                        f"namespace {namespace} {{", "namespace {", "struct Scene;",
                        "Scene* active = nullptr;", "void (*navigate)(const char*, bool) = nullptr;",
                        *constants, "struct Scene", "{", block(class_content), "};",
                        *[members(code, "active->") for code in hooks], "",
                        "Scene::Scene(ComponentRegistry& parent, PixmapRect viewport)",
                        "    : " + ",\n      ".join(["screen(&parent)", *initialization]),
                        "{", "    active = this;", block(setup), "}", "}",
                        "const PanelMenuView& descriptor()", "{",
                        f"    static const auto view = PanelMenuView::make<Scene>({cpp(name)});",
                        "    return view;", "}", "",
                        "void onNavigate(void (*callback)(const char*, bool)) { navigate = callback; }",
                        "}", ""])
    images = [GENERATED.rstrip(), "#pragma once", "#include <Arduino.h>", ""]
    for raster in builder.rasters.values():
        images.extend(raster.code())
    target = Path(target).resolve()
    target.mkdir(parents=True, exist_ok=True)
    outputs = {f"{base}.h": header, f"{base}.cpp": source, f"{base}_font.h": GENERATED + font,
               f"{base}_images.h": "\n".join(images) + "\n",
               f"{name}.pkm": f"PKM1\nTITLE\t{title}\nVIEW\t{name}\n"}
    if license_text:
        outputs[f"{base}_font-license.txt"] = license_text
    for filename, data in outputs.items():
        existing = target / filename
        if not existing.exists():
            continue
        content = existing.read_text(encoding="utf-8-sig")
        same_view = (filename == f"{name}.pkm" and content.startswith("PKM1\nTITLE\t")
                     and content.endswith(f"\nVIEW\t{name}\n") and len(content.splitlines()) == 3)
        if content != data and not content.startswith(GENERATED) and not same_view:
            raise ValueError(f"Export would replace a user file: {filename}.")
    for filename, data in outputs.items():
        atomic_text(target / filename, data)
    return tuple(target / filename for filename in outputs)
