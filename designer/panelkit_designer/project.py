"""Versioned project documents and validation shared by the editor and CLI."""

import copy
from dataclasses import asdict, dataclass, field
from datetime import date
import json
import math
from pathlib import Path
import re

from .catalog import THEME_FIELDS, WIDGETS

ID = re.compile(r"[a-zA-Z][a-zA-Z0-9_]{0,39}\Z")
COLOR = re.compile(r"#[0-9a-fA-F]{6}\Z")
SYMBOL = re.compile(r"(?:::)?[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*\Z")
RESERVED = set("""alignas alignof and asm auto bool break case catch char char16_t char32_t
class compl concept const constexpr const_cast continue co_await co_return co_yield decltype
default delete do double dynamic_cast else enum explicit export extern false float for friend
goto if inline int long mutable namespace new noexcept not nullptr operator or private protected
public register reinterpret_cast requires return short signed sizeof static static_assert
static_cast struct switch template this thread_local throw true try typedef typeid typename
union unsigned using virtual void volatile wchar_t while xor and_eq not_eq or_eq xor_eq
min max abs round F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 HIGH LOW INPUT OUTPUT PI""".split())


@dataclass
class Widget:
    kind: str
    name: str
    x: int = 12
    y: int = 12
    width: int = 100
    height: int = 24
    parent: str = ""
    visible: bool = True
    active: bool = True
    order: int = 0
    properties: dict = field(default_factory=dict)
    events: dict = field(default_factory=dict)

    @classmethod
    def create(cls, kind, name, x=12, y=12, parent=""):
        schema = WIDGETS[kind]
        return cls(kind, name, x, y, *schema.size, parent,
                   properties={key: copy.deepcopy(item.default) for key, item in schema.fields.items()})


@dataclass
class Project:
    width: int = 320
    height: int = 240
    background: str = "#0e1723"
    buffered: bool = False
    theme: dict = field(default_factory=dict)
    widgets: list[Widget] = field(default_factory=list)
    font_header: str = ""
    font_symbol: str = ""
    pins: list[int] = field(default_factory=lambda: [10, 9, 255, 8, 2])
    drag_threshold: int = 8
    double_click_ms: int = 300
    deferred_clicks: bool = False
    includes: str = ""
    configure_cpp: str = ""
    frame_cpp: str = ""
    path: Path | None = field(default=None, repr=False, compare=False)

    def to_dict(self):
        data = asdict(self)
        del data["path"]
        return {"version": 1, **data}

    @classmethod
    def from_dict(cls, data, path=None):
        data = dict(data)
        if data.pop("version", None) != 1:
            raise ValueError("Unsupported PanelKit project version.")
        widgets = [Widget(**entry) for entry in data.pop("widgets", [])]
        project = cls(**data, widgets=widgets, path=path)
        validate(project)
        return project

    def asset(self, name):
        path = Path(name).expanduser()
        if not path.is_absolute() and self.path:
            path = self.path.parent / path
        return path.resolve()

    def widget(self, name):
        return next((widget for widget in self.widgets if widget.name == name), None)

    def ordered(self):
        children = {}
        for widget in self.widgets:
            children.setdefault(widget.parent, []).append(widget)

        def visit(parent):
            for child in children.get(parent, []):
                yield child
                yield from visit(child.name)

        return list(visit(""))


def values(widget):
    return {name: widget.properties.get(name, item.default)
            for name, item in WIDGETS[widget.kind].fields.items()}


def atomic_text(path, text):
    import os
    from tempfile import NamedTemporaryFile

    with NamedTemporaryFile("w", encoding="utf-8", newline="\n", dir=path.parent,
                            delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(text)
            stream.close()
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def save_project(project, path):
    validate(project)
    path = Path(path).resolve()
    atomic_text(path, json.dumps(project.to_dict(), indent=2, ensure_ascii=False) + "\n")
    project.path = path
    return path


def load_project(path):
    path = Path(path).resolve()
    return Project.from_dict(json.loads(path.read_text(encoding="utf-8-sig")), path)


def check(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def numeric(value):
    return type(value) in (int, float) and abs(value) <= 1e30 and math.isfinite(value)


def color(value):
    return isinstance(value, str) and bool(COLOR.fullmatch(value))


def sequence(value, length, test):
    return isinstance(value, list) and len(value) == length and all(map(test, value))


def validate_theme(theme):
    check(isinstance(theme, dict), "Theme must be a JSON object.")
    for key, value in theme.items():
        kind = THEME_FIELDS.get(key)
        ok = ((kind == "color" and color(value))
              or (kind == "int" and integer(value, 0, 512))
              or (key == "font_filter" and value in ("PRESERVE_STROKES", "NEAREST")))
        check(ok, f"Invalid theme property {key}.")


def validate_field(name, value, item):
    kind, default = item.kind, item.default
    if kind == "theme":
        validate_theme(value)
        return True
    if kind == "color":
        return color(value)
    if kind == "colors":
        return isinstance(value, list) and len(value) <= 4 and all(map(color, value))
    if kind == "date":
        if not sequence(value, 3, lambda n: type(n) is int):
            return False
        try:
            date(*value)
        except (ValueError, OverflowError):
            return False
        return True
    if kind == "items":
        return isinstance(value, list) and all(isinstance(v, str) for v in value)
    if kind == "rows":
        return isinstance(value, list) and all(isinstance(row, list) and
            all(isinstance(v, str) for v in row) for row in value)
    if kind == "columns":
        return isinstance(value, list) and all(isinstance(row, list) and len(row) == 2
            and isinstance(row[0], str) and integer(row[1], 1, 32767) for row in value)
    if kind == "pages":
        return isinstance(value, list) and all(sequence(row, 2, lambda v: isinstance(v, str))
                                               for row in value)
    if kind == "layout":
        return isinstance(value, list) and all(isinstance(row, list) and len(row) == 3
            and isinstance(row[0], str) and all(integer(n, 0, 32767) for n in row[1:])
            for row in value)
    if kind == "samples":
        return isinstance(value, list) and all(map(numeric, value))
    if kind == "region":
        return value == [] or (sequence(value, 4, lambda n: integer(n, 0, 32767))
                               and min(value[2:]) > 0)
    if kind == "pose":
        allowed = {"x", "y", "pivotX", "pivotY", "angle", "scaleX", "scaleY"}
        return isinstance(value, dict) and value.keys() <= allowed and all(map(numeric, value.values()))
    if " " in kind:
        return isinstance(value, str) and value in kind.split()
    if isinstance(default, list):
        if name == "range" and len(default) == 2:
            return sequence(value, 2, numeric) and value[0] < value[1]
        if name == "range":
            return (sequence(value, 3, lambda n: integer(n, -32768, 32767))
                    and value[0] <= value[1] and value[2] > 0)
        return sequence(value, len(default), lambda n: isinstance(n, type(default[0]))
                        and type(n) is type(default[0]) and (type(n) is bool or abs(n) <= 32767))
    if type(default) is bool:
        return type(value) is bool
    if type(default) is int:
        if name == "value":
            return integer(value, -32768, 32767)
        lower = {"capacity": 2, "interval_ms": 1, "font_height": 1, "row_height": 1,
                 "popup_rows": 1}.get(name, 0)
        return integer(value, lower, 32767)
    if type(default) is float:
        return numeric(value) and (name not in ("step", "font_scale") or value >= 0)
    return isinstance(value, str)


def validate(project):
    check(integer(project.width, 1, 4096) and integer(project.height, 1, 4096),
          "Screen dimensions must be between 1 and 4096 pixels.")
    check(color(project.background), "Screen background must be #RRGGBB.")
    validate_theme(project.theme)
    check(sequence(project.pins, 5, lambda pin: integer(pin, 0, 255)), "Enter five pin numbers.")
    check(integer(project.drag_threshold, 0, 32767) and integer(project.double_click_ms, 0, 32767),
          "Invalid touch timing or drag threshold.")
    check(all(type(flag) is bool for flag in (project.buffered, project.deferred_clicks)),
          "Screen buffering and deferred click settings must be Boolean.")
    check(all(isinstance(s, str) for s in (project.font_header, project.font_symbol,
          project.includes, project.configure_cpp, project.frame_cpp)), "Invalid project text.")
    check(bool(project.font_header) == bool(project.font_symbol)
          and (not project.font_symbol or SYMBOL.fullmatch(project.font_symbol)),
          "Specify a font header and its qualified C++ symbol together.")
    names = set()
    for widget in project.widgets:
        check(widget.kind in WIDGETS, f"Unknown widget type {widget.kind}.")
        check(isinstance(widget.name, str) and ID.fullmatch(widget.name)
              and widget.name not in RESERVED and "__" not in widget.name,
              f"Invalid C++ widget ID {widget.name!r}.")
        check(widget.name not in names, f"Duplicate widget ID {widget.name}.")
        names.add(widget.name)
        check(integer(widget.width, 1, 4096) and integer(widget.height, 1, 4096)
              and integer(widget.x, -32768, 32767) and integer(widget.y, -32768, 32767)
              and integer(widget.order, 0, 32767), f"{widget.name}: invalid geometry or order.")
        check(type(widget.visible) is bool and type(widget.active) is bool
              and isinstance(widget.parent, str), f"{widget.name}: invalid state.")
        schema = WIDGETS[widget.kind]
        check(isinstance(widget.properties, dict) and widget.properties.keys() <= schema.fields.keys(),
              f"{widget.name}: unknown property.")
        for key, value in values(widget).items():
            check(validate_field(key, value, schema.fields[key]), f"{widget.name}: invalid {key}.")
        check(isinstance(widget.events, dict) and widget.events.keys() <= schema.events.keys()
              and all(isinstance(body, str) for body in widget.events.values()),
              f"{widget.name}: invalid event callback.")
        data = values(widget)
        if widget.kind == "Table":
            check(all(len(row) == len(data["columns"]) for row in data["rows"]),
                  f"{widget.name}: every row must match the number of columns.")
        if data.get("colors"):
            check(len(data["colors"]) in ((2, 3, 4) if widget.kind == "Button" else (2,)),
                  f"{widget.name}: wrong number of colors.")

    for widget in project.widgets:
        ancestor = widget
        visited = {widget.name}
        while ancestor.parent:
            ancestor = project.widget(ancestor.parent)
            check(ancestor and WIDGETS[ancestor.kind].container,
                  f"{widget.name}: parent must be a container.")
            check(ancestor.name not in visited, f"{widget.name}: parent cycle.")
            visited.add(ancestor.name)
        data = values(widget)
        if widget.kind == "Tabs":
            pages = [name for _, name in data["pages"]]
            check(len(pages) == len(set(pages)) and all(
                (page := project.widget(name)) and WIDGETS[page.kind].container
                and page.parent == widget.parent for name in pages),
                f"{widget.name}: tab pages must be distinct sibling containers.")
        if widget.kind == "Flex":
            children = [name for name, _, _ in data["layout"]]
            check(len(children) == len(set(children)) and all(
                (child := project.widget(name)) and child.parent == widget.name for name in children),
                f"{widget.name}: flex entries must be distinct direct children.")

