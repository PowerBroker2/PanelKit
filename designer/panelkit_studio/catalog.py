"""Editor properties and the associated PanelKit API signatures."""

from dataclasses import dataclass


@dataclass(frozen=True)
class Field:
    default: object
    method: str = ""
    kind: str = ""


def props(**fields):
    return {name: Field(*data) for name, data in fields.items()}


@dataclass(frozen=True)
class WidgetType:
    cpp: str
    size: tuple[int, int]
    fields: dict
    events: dict
    container: bool = False
    direct: bool = True

    def __post_init__(self):
        object.__setattr__(self, "events", self.events | {"onInput": "const Event& event"})


THEME = props(
    theme=({}, "", "theme"),
)
BACKGROUND = props(background=("#192635", "setBackground", "color"))
GRID = props(grid_columns=(0,), grid_gap=([4, 4],), grid_padding=(0,))
TEXT = props(text=("Text", "setText"))
CHECK = props(checked=(False, "setChecked"))
CHOICES = props(items=(["First", "Second", "Third"], "", "items"),
                selected=(0, "setSelected"))
COLORS = props(colors=([], "setColors", "colors"))
BUFFERED = props(buffered=(False,))
FONT = props(font_height=(14,))
COMPONENT_EVENTS = dict.fromkeys(
    ["onClick", "onDoubleClick", "onScroll", "onType", "onPress", "onRelease", "onDrag"], ""
)
ASSET = props(image=("", "", "image"))
BLIT_MODES = "TOP_LEFT TOP_RIGHT BOTTOM_RIGHT BOTTOM_LEFT CENTER FIT FILL STRETCH"
IMAGE_MODES = props(mode=("FIT", "setMode", BLIT_MODES))
CHANGE = {"onChange": "int value"}

WIDGETS = {
    "Panel": WidgetType("Panel", (180, 120), BACKGROUND | BUFFERED | GRID, {}, True),
    "Flex": WidgetType("PanelFlex", (180, 120), BACKGROUND | props(
        axis=("VERTICAL", "", "VERTICAL HORIZONTAL"), gap=(4,), padding=(4,),
        layout=([], "", "layout")), {}, True),
    "Window": WidgetType("PanelWindow", (196, 140), THEME | GRID | props(
        title=("Inspector", "setTitle"), features=([True, True, True], "setFeatures"),
        min_size=([80, 48], "setMinSize"), snap_distance=(12, "setSnapDistance"),
        raise_on_press=(True, "setRaiseOnPress"), fit_single_child=(False, "fitSingleChild"),
        minimized=(False, "setMinimized"), maximized=(False, "setMaximized"),
        modal=(False, "setModal")), {"onClose": ""}, True),
    "Scroll Area": WidgetType("PanelScrollArea", (176, 102), BACKGROUND | props(
        content_size=([176, 200], "setContentSize"), scroll=([0, 0], "setScroll")), {}, True),
    "Tabs": WidgetType("PanelTabs", (176, 24), THEME | props(
        pages=([], "", "pages"), selected=(0, "setCurrent")), CHANGE),
    "Text": WidgetType("PanelText", (112, 22), THEME | TEXT | props(
        auto_size=(False, "setAutoSize")), {}),
    "Label": WidgetType("PanelLabel", (112, 22), TEXT | FONT | props(
        color=("#ffffff", "setColor", "color")), {}, direct=False),
    "Button": WidgetType("PanelButton", (86, 30), TEXT | BUFFERED | COLORS | props(
        font_scale=(0.72,), justify=("CENTER", "", "LEFT CENTER RIGHT"),
        vertical_align=("MIDDLE", "", "TOP MIDDLE BOTTOM"),
        corner_radius=(4, "setCornerRadius"), text_padding=(4, "setPadding"),
        menu_link=("",), menu_action=("",)), {"onClick": "", "onDoubleClick": ""}, direct=False),
    "Check Box": WidgetType("PanelCheckBox", (120, 24), THEME | TEXT | CHECK,
                            {"onChange": "bool checked"}),
    "Radio Group": WidgetType("PanelRadioGroup", (120, 72), THEME | CHOICES, CHANGE),
    "Combo Box": WidgetType("PanelComboBox", (120, 26), THEME | CHOICES | props(
        popup_rows=(4, "setPopupRows")), CHANGE),
    "List": WidgetType("PanelList", (120, 86), props(
        items=(["First", "Second", "Third"], "", "items"), row_height=(22,)), CHANGE, direct=False),
    "Slider": WidgetType("PanelSlider", (126, 24), COLORS | props(
        range=([0.0, 1.0], "setRange"), step=(0.0, "setStep"), value=(0.5, "setValue")),
        {"onChange": "float value"}),
    "Progress": WidgetType("PanelProgress", (126, 14), COLORS | props(
        value=(0.6, "setValue")), {}),
    "Toggle": WidgetType("PanelToggle", (52, 24), COLORS | CHECK, {"onChange": "bool checked"}),
    "Plot": WidgetType("PanelPlot", (172, 88), BUFFERED | COLORS | props(
        capacity=(48,), range=([-1.0, 1.0], "setRange"), visible_samples=(48, "setVisibleSamples"),
        samples=([], "", "samples"), demo_signal=(False,), interval_ms=(60,)), {}),
    "Table": WidgetType("PanelTable", (168, 95), THEME | props(
        columns=([["Name", 85], ["Reading", 83]], "", "columns"),
        rows=([["Input A", "1.25 V"], ["Input B", "2.70 V"]], "", "rows"),
        row_height=(20, "setRowHeight"), header_height=(20, "setHeaderHeight"),
        scroll=([0, 0], "setScroll")), {"onCellClick": "int row, int column"}),
    "Calendar": WidgetType("PanelCalendar", (174, 148), THEME | props(
        date=([2026, 1, 1], "setDate", "date")), {"onChange": "PanelDate date"}),
    "Spin Box": WidgetType("PanelSpinBox", (100, 25), THEME | props(
        range=([0, 100, 1], "setRange"), value=(0, "setValue")), CHANGE),
    "Text Field": WidgetType("PanelTextField", (125, 27), THEME | TEXT,
                              {"onChange": "const char* text"}),
    "Keypad": WidgetType("PanelKeypad", (96, 84), THEME, {"onKey": "char key"}),
    "Picture": WidgetType("PanelPicture", (126, 86), ASSET | IMAGE_MODES, {}),
    "Sprite": WidgetType("PanelSprite", (96, 96), ASSET | props(
        region=([], "", "region"),
        pose=({"x": 48, "y": 48, "angle": 0, "scaleX": 1, "scaleY": 1}, "", "pose")),
        {}, direct=False),
    "Pixmap": WidgetType("Component", (126, 86), ASSET | props(
        mode=("FIT", "", BLIT_MODES), filter=("BILINEAR", "", "NEAREST BILINEAR AREA")),
        COMPONENT_EVENTS, direct=False),
}

THEME_FIELDS = {
    "background": "color", "surface": "color", "border": "color", "accent": "color",
    "text": "color", "padding": "int", "text_height": "int",
    "font_filter": "PRESERVE_STROKES NEAREST",
}

