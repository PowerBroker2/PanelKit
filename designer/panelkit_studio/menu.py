"""Export a small declarative page for the native PanelMenuNavigator."""

from pathlib import Path
import re

from .catalog import WIDGETS
from .project import atomic_text, validate, values

NAME = re.compile(r"[A-Za-z0-9_-]+")
FILENAME = re.compile(r"[A-Za-z0-9_-]+\.pkm")


def field(value, limit, label):
    if not value or len(value) > limit or any(not 32 <= ord(char) <= 126 for char in value):
        raise ValueError(f"{label} must use 1–{limit} printable ASCII characters.")
    return value


def link_path(value):
    field(value, 79, "Menu link")
    parts = value.removeprefix("/").split("/")
    if not FILENAME.fullmatch(parts[-1]) or any(
        item not in (".", "..") and not NAME.fullmatch(item) for item in parts[:-1]
    ):
        raise ValueError("Menu links must target a .pkm file with ASCII path segments.")
    if value.startswith("/"):
        depth = 0
        for item in parts[:-1]:
            if item == "..":
                depth -= 1
                if depth < 0:
                    raise ValueError("The link traverses above the menu root.")
            elif item != ".":
                depth += 1
    return value


def render_menu_page(project, title, section=""):
    """Flatten a screen or selected container's visible direct children into PKM1."""
    validate(project)
    if section:
        container = project.widget(section)
        if not container or not WIDGETS[container.kind].container:
            raise ValueError("Select a container to export as a menu page.")
    children = sorted((widget for widget in project.widgets if widget.parent == section and widget.visible),
                      key=lambda widget: (widget.y, widget.x))
    text_lines = []
    items = []
    for widget in children:
        data = values(widget)
        if widget.kind in ("Text", "Label"):
            if data["text"]:
                text_lines.append(field(data["text"], 47, widget.name))
        elif widget.kind == "Button":
            link, action = data["menu_link"], data["menu_action"]
            if bool(link) == bool(action):
                raise ValueError(f"{widget.name} needs exactly one Menu link or Menu action.")
            label = field(data["text"], 39, widget.name)
            if action:
                if not NAME.fullmatch(field(action, 79, "Menu action")):
                    raise ValueError(f"{widget.name} has an invalid action name.")
                items.append(f"ACTION\t{label}\t{action}")
            else:
                items.append(f"LINK\t{label}\t{link_path(link)}")
        else:
            raise ValueError(f"{widget.name} is a {widget.kind}. Menu pages support text and buttons.")
    if len(text_lines) > 2 or len(items) > 5:
        raise ValueError("A menu page supports up to two text rows and five buttons.")
    source = "\n".join(["PKM1", "TITLE\t" + field(title, 39, "Page title"),
                        *("TEXT\t" + line for line in text_lines), *items]) + "\n"
    if len(source) > 512:
        raise ValueError("A PKM1 page cannot exceed 512 bytes.")
    return source


def export_menu_page(project, path, title, section=""):
    """Write a validated .pkm file and return its path."""
    target = Path(path).resolve()
    if not FILENAME.fullmatch(target.name):
        raise ValueError("The filename must use ASCII letters, digits, dashes, or underscores and end in .pkm.")
    atomic_text(target, render_menu_page(project, title, section))
    return target
