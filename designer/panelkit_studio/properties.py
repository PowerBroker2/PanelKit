"""Typed property controls for scalars, records, and editable tables."""

import copy
import calendar
from dataclasses import dataclass
from datetime import date
import math
import tkinter as tk
from tkinter import colorchooser, messagebox, ttk

from .catalog import THEME_FIELDS

CARD = "Card.TFrame"
LABEL = "Card.TLabel"


def numbers(frame, value, low=0, high=32767, step=None):
    variable = tk.StringVar(value=str(value))
    floating = type(value) is float or isinstance(step, float)
    cast = float if floating else int

    def valid(text):
        if text in ("", "-") or (floating and text in (".", "-.")):
            return True
        try:
            number = cast(text)
            return math.isfinite(number)
        except (ValueError, OverflowError):
            return False

    box = ttk.Spinbox(frame, from_=low, to=high, increment=step or 1, textvariable=variable,
                      validate="key", validatecommand=(frame.register(valid), "%P"), width=9)

    def read():
        number = cast(variable.get())
        if not low <= number <= high:
            raise ValueError(f"Enter a value between {low} and {high}.")
        return number

    return box, read


class RecordValue:
    def __init__(self, value):
        self.value = copy.deepcopy(value)

    def get(self):
        return copy.deepcopy(self.value)


@dataclass(frozen=True)
class Column:
    label: str
    default: object
    options: tuple = ()
    bounds: tuple = ()
    color: bool = False

    def input(self, container, value):
        if self.options:
            box = ttk.Combobox(container, values=self.options, state="readonly", width=14)
            box.set(value)
            return box, box.get
        if type(self.default) is bool:
            variable = tk.BooleanVar(value=value)
            return ttk.Checkbutton(container, variable=variable), variable.get
        if isinstance(self.default, (int, float)):
            option = self.bounds or ((-1000000, 1000000, 0.1) if isinstance(self.default, float)
                                     else (0, 32767))
            return numbers(container, value, *option)
        if self.color:
            button = ttk.Button(container, text=value, width=11)

            def pick():
                _, selected = colorchooser.askcolor(button.cget("text"), parent=button)
                if selected:
                    button.configure(text=selected)
            button.configure(command=pick)
            return button, lambda: str(button.cget("text"))
        field = ttk.Entry(container, width=16)
        field.insert(0,value)
        return field, field.get


FIXED = {
    "features": [Column(label,True) for label in ("Movable","Resizable","Closable")],
    "min_size": [Column(label,1,bounds=(1,4096)) for label in ("Width","Height")],
    "content_size": [Column(label,0) for label in ("Width","Height")],
    "grid_gap": [Column(label,4) for label in ("Horizontal gap","Vertical gap")],
    "scroll": [Column(label,0) for label in ("Horizontal offset","Vertical offset")],
    "pins": [Column(label,pin,bounds=(0,255)) for label,pin in
             zip(("Display CS","Display DC","Reset","Touch CS","Touch IRQ"),(10,9,255,8,2))],
    "region": [Column("Left",0),Column("Top",0),Column("Width",1,bounds=(1,32767)),
               Column("Height",1,bounds=(1,32767))],
}
THEME_DEFAULTS = {"background":"#192635", "surface":"#223345", "border":"#3e5163",
                  "accent":"#3ea7a2", "text":"#ffffff", "padding":5, "text_height":14,
                  "font_filter":"PRESERVE_STROKES"}


class PropertyDialog:
    def __init__(self, parent, name, accept):
        self.window = tk.Toplevel(parent)
        self.window.title(name)
        self.window.transient(parent)
        frame = ttk.Frame(self.window, style=CARD, padding=12)
        frame.pack(fill="both",expand=True)
        self.controls = ttk.Frame(frame, style=CARD)
        self.controls.pack(fill="x")
        viewer = ttk.Frame(frame, style=CARD)
        viewer.pack(fill="both",expand=True,pady=(0,10))
        self.view = tk.Canvas(viewer, background="#1a2634", highlightthickness=0, height=255, width=590)
        scroll = ttk.Scrollbar(viewer, orient="vertical", command=self.view.yview)
        scroll.pack(side="right",fill="y")
        self.view.pack(side="left",fill="both",expand=True)
        self.view.configure(yscrollcommand=scroll.set)
        self.body = ttk.Frame(self.view, style=CARD)
        inner = self.view.create_window((0,0), window=self.body, anchor="nw")
        self.body.bind("<Configure>",lambda _: self.view.configure(scrollregion=self.view.bbox("all")))
        self.view.bind("<Configure>",lambda event: self.view.itemconfigure(inner,width=event.width))
        buttons = ttk.Frame(frame,style=CARD)
        buttons.pack(fill="x")
        ttk.Button(buttons,text="Cancel",command=self.window.destroy).pack(side="right")
        ttk.Button(buttons,text="Use values",style="Accent.TButton",command=accept).pack(side="right",padx=6)

    def modal(self):
        self.window.grab_set()
        self.window.bind("<Escape>",lambda _: self.window.destroy())

    def finish(self, callback, result):
        try:
            callback(result())
        except (ValueError, TypeError) as error:
            messagebox.showerror("Check the values",str(error),parent=self.window)
        else:
            self.window.destroy()


def form(parent, title, fields, value, callback, *, mode="fixed", validator=None):
    """Edit a fixed tuple, optional record, or collection with column inputs."""
    readers = []
    dialog = PropertyDialog(parent,title,lambda: dialog.finish(callback, collect))
    optional = tk.BooleanVar(value=bool(value))
    toggles = []
    if mode == "optional":
        ttk.Checkbutton(dialog.controls,text="Use a custom crop",variable=optional).pack(anchor="w")
    count = tk.StringVar(value=str(len(value)))
    if mode == "colors":
        choices = ("0","2","3","4") if len(fields) == 4 else ("0","2")
        ttk.Label(dialog.controls,text="Colors to override",style=LABEL).pack(side="left",padx=(0,8))
        ttk.Combobox(dialog.controls,state="readonly",textvariable=count,width=5,
                     values=choices).pack(side="left")
    for index, (key,column) in enumerate(fields):
        if mode == "record":
            start = value.get(key,column.default)
            enabled = tk.BooleanVar(value=key in value)
            toggles.append(enabled)
            ttk.Checkbutton(dialog.body,text="Override",variable=enabled).grid(row=index,column=2,padx=7)
        else:
            start = value[index] if index < len(value) else column.default
        ttk.Label(dialog.body,text=column.label,style=LABEL).grid(row=index,column=0,
                                                                sticky="w",pady=4,padx=(0,10))
        control, reader = column.input(dialog.body,start)
        control.grid(row=index,column=1,sticky="ew",pady=4)
        readers.append((key,reader))
    dialog.body.columnconfigure(1,weight=1)

    def collect():
        if mode == "record":
            result = {key: reader() for (key,reader), flag in zip(readers,toggles) if flag.get()}
        elif mode == "optional" and not optional.get():
            result = []
        elif mode == "colors":
            result = [reader() for _,reader in readers[:int(count.get())]]
        else:
            result = [reader() for _,reader in readers]
        if validator:
            validator(result)
        return result

    dialog.modal()


def date_picker(parent, title, value, callback):
    dialog = PropertyDialog(parent,title,lambda: dialog.finish(callback, collect))
    controls = dialog.body
    year, read_year = numbers(controls,value[0],1,9999)
    month = ttk.Combobox(controls,state="readonly",values=tuple(calendar.month_name)[1:])
    month.current(value[1]-1)
    day, read_day = numbers(controls,value[2],1,31)
    for i,(label,control) in enumerate((("Year",year),("Month",month),("Day",day))):
        ttk.Label(controls,text=label,style=LABEL).grid(row=i,column=0,sticky="w",padx=(0,10),pady=4)
        control.grid(row=i,column=1,sticky="ew",pady=4)
    controls.columnconfigure(1,weight=1)

    def scrub(_=None):
        try:
            limit = calendar.monthrange(read_year(),month.current()+1)[1]
            day.configure(to=limit)
            if read_day() > limit:
                day.set(str(limit))
        except ValueError:
            return

    def collect():
        scrub()
        chosen = [read_year(),month.current()+1,read_day()]
        date(*chosen)
        return chosen

    month.bind("<<ComboboxSelected>>",scrub)
    year.bind("<KeyRelease>",scrub)
    year.configure(command=scrub)
    scrub()
    dialog.modal()


def table(parent, title, columns, value, callback, *, flat=False, unique=None, origins=False):
    rows = []
    dialog = PropertyDialog(parent,title,lambda: dialog.finish(callback, collect))
    header = ttk.Frame(dialog.body,style=CARD)
    header.pack(fill="x")
    for index,column in enumerate(columns):
        ttk.Label(header,text=column.label,style=LABEL,width=15).grid(row=0,column=index,sticky="w")

    def reorder(row, delta):
        old = rows.index(row)
        new = old + delta
        if 0 <= new < len(rows):
            rows[old],rows[new] = rows[new],rows[old]
            for container,_,_ in rows:
                container.pack_forget()
                container.pack(fill="x",pady=2)

    def delete(row):
        rows.remove(row)
        row[0].destroy()

    def append(values=None,origin=None):
        line = ttk.Frame(dialog.body,style=CARD)
        line.pack(fill="x",pady=2)
        values = values if values is not None else [col.default for col in columns]
        readers = []
        for index,column in enumerate(columns):
            cell,reader = column.input(line,values[index] if index < len(values) else column.default)
            cell.grid(row=0,column=index,sticky="ew",padx=(0,3))
            line.columnconfigure(index,weight=1)
            readers.append(reader)
        row = (line,readers,origin)
        rows.append(row)
        for index,(text,action) in enumerate((("↑",lambda: reorder(row,-1)),
                                             ("↓",lambda: reorder(row,1)),("×",lambda: delete(row)))):
            ttk.Button(line,text=text,width=2,command=action).grid(row=0,column=len(columns)+index)

    def collect():
        values = [[reader() for reader in readers] for _,readers,_ in rows]
        if unique is not None and len({row[unique] for row in values}) != len(values):
            raise ValueError("Use each widget only once.")
        if origins:
            return values, [origin for _,_,origin in rows]
        return [row[0] for row in values] if flat else values

    if columns:
        ttk.Button(dialog.controls,text="Add row",command=append).pack(anchor="w",pady=(0,7))
        for i,row in enumerate(value):
            append([row] if flat else row,i)
    else:
        ttk.Label(dialog.body,text="Add columns or linked widgets first.",style=LABEL).pack(anchor="w")
    dialog.modal()


def edit_record(parent, name, value, callback, *, widget=None, children=(), columns=()):
    name_label = name.replace("_"," ").capitalize()
    if name == "theme":
        fields = [(key,Column(key.replace("_"," ").capitalize(),THEME_DEFAULTS[key],
                  tuple(kind.split()) if " " in kind else (), (0,512) if kind == "int" else (),
                  kind == "color")) for key,kind in THEME_FIELDS.items()]
        return form(parent,name_label,fields,value,callback,mode="record")
    if name == "pose":
        fields = [(key,Column(label,default,bounds=(-10000.0,10000.0,0.1))) for key,label,default in (
            ("x","Destination X",widget.width/2),("y","Destination Y",widget.height/2),
            ("pivotX","Source pivot X",0.0),("pivotY","Source pivot Y",0.0),
            ("angle","Angle",0.0),("scaleX","Scale X",1.0),("scaleY","Scale Y",1.0))]
        return form(parent,name_label,fields,value,callback,mode="record")
    if name == "colors":
        names = ("Background","Text","Pressed","Border") if widget.kind == "Button" else ("Track / background","Fill / trace")
        defaults = ("#424142","#ffffff","#212021","#7b7d7b") if widget.kind == "Button" else ("#253445","#3ea7a2")
        fields = [(i,Column(label,default,color=True)) for i,(label,default) in enumerate(zip(names,defaults))]
        return form(parent,name_label,fields,value,callback,mode="colors")
    if name == "range":
        spin = widget.kind == "Spin Box"
        fields = [Column(label,default,bounds=(-32768,32767) if spin else (-1000000.0,1000000.0,0.1))
                  for label,default in zip(("Minimum","Maximum"),value)]
        if spin:
            fields.append(Column("Increment",value[2],bounds=(1,32767)))
        def validate_range(items):
            if items[0] > items[1] or (not spin and items[0] == items[1]):
                raise ValueError("Choose a maximum above the minimum.")
        return form(parent,name_label,list(enumerate(fields)),value,callback,validator=validate_range)
    if name == "date":
        return date_picker(parent,name_label,value,callback)
    if name in FIXED:
        return form(parent,name_label,list(enumerate(FIXED[name])),value,callback,
                    mode="optional" if name == "region" else "fixed")
    if name == "items":
        return table(parent,name_label,[Column("Caption","Item")],value,callback,flat=True)
    if name == "samples":
        return table(parent,name_label,[Column("Sample",0.0)],value,callback,flat=True)
    if name == "columns":
        return table(parent,name_label,[Column("Header","Column"),Column("Width",80,bounds=(1,32767))],
                     value,callback,origins=True)
    if name == "rows":
        return table(parent,name_label,[Column(header,"") for header,_ in columns],value,callback)
    if name in ("pages","layout") and children:
        widget_choice = Column("Widget",children[0],tuple(children))
        fields = ([Column("Caption","Page"),widget_choice] if name == "pages" else
                  [widget_choice,Column("Basis",0),Column("Weight",1)])
        return table(parent,name_label,fields,value,callback,unique=1 if name == "pages" else 0)
    return table(parent,name_label,[],value,callback)
