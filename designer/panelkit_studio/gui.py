"""Desktop editor for PanelKit project files."""

import copy
import os
from pathlib import Path
import re
import subprocess
import sys
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, simpledialog, ttk

from .catalog import WIDGETS
from .menu import export_menu_page
from .menu_view import export_menu_view
from .properties import RecordValue, edit_record, numbers
from .typefaces import TypefaceDialog, require_glyph_bridge
from .export import export_project, SOURCE, cpp
from .preview import Preview
from .project import SERIAL_FORMATS, SERIAL_PORTS, Project, Widget, load_project, save_project, validate, values

SHELL = "#111923"
CARD = "#1a2634"
FIELD = "#233346"
INK = "#e8f0f6"
DIM = "#9eb0c2"
MINT = "#82e7cd"
ZOOMS = ("75%", "100%", "125%", "150%", "200%", "250%", "300%")


ICONS = dict(zip(WIDGETS, ("▣","↕","▤","⇳","◫","≡","Aa","▰","☑","◉","▾","☷",
                          "⇆","▥","⏻","∿","▦","▧","±","☰","⠿","▨","◇","▩")))
PALETTE = {f"{ICONS[kind]}  {kind}": kind for kind in WIDGETS}

NUMBERS = {
    "serial_baud":(300,8000000,300), "serial_budget":(1,1024),
    "x":(-32768,32767), "y":(-32768,32767), "width":(1,4096), "height":(1,4096),
    "capacity":(2,32767), "font_height":(1,512), "row_height":(1,512), "popup_rows":(1,32),
    "interval_ms":(1,32767), "font_scale":(0.0,16.0,0.05), "step":(0.0,1000000.0,0.1),
}


class Reader:
    def __init__(self, get):
        self.get = get


class Studio:
    def __init__(self, root, project):
        self.root, self.project = root, project
        self.selection = ""
        self.saved = project.to_dict()
        self.history = []
        self.redo_history = []
        self.zoom = tk.StringVar(value="150%")
        self.status = tk.StringVar(value="Choose a widget to add, or open a project.")
        self.form = {}
        self.gesture = None
        self.last_export = None
        self.building = False
        self.configure_style()
        self.build()
        self.refresh()

    def configure_style(self):
        self.root.configure(bg=SHELL)
        self.root.geometry("1370x850")
        self.root.minsize(940, 600)
        self.root.option_add("*Font", ("Segoe UI", 10))
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure(".", background=SHELL, foreground=INK, bordercolor=FIELD,
                        darkcolor=FIELD, lightcolor=FIELD, troughcolor=SHELL,
                        fieldbackground=FIELD, selectbackground="#31536c",
                        selectforeground=INK, insertcolor=INK)
        style.configure("TFrame", background=SHELL)
        style.configure("Card.TFrame", background=CARD)
        style.configure("Card.TLabel", background=CARD, foreground=INK)
        style.configure("Hint.TLabel", background=CARD, foreground=DIM)
        style.configure("Brand.TLabel", font=("Segoe UI", 19, "bold"), foreground=INK)
        style.configure("Title.TLabel", font=("Segoe UI", 12, "bold"), foreground=INK,
                        background=CARD)
        style.configure("TButton", padding=(10, 5), background=FIELD)
        style.map("TButton", background=[("active", "#304960")])
        style.configure("Accent.TButton", background=MINT, foreground=SHELL,
                        font=("Segoe UI", 10, "bold"))
        style.map("Accent.TButton", background=[("active", "#abf4dd")])
        style.configure("TEntry", padding=5)
        style.configure("TCombobox", padding=4)
        style.map("TCombobox", fieldbackground=[("readonly", FIELD)],
                  foreground=[("readonly", INK)])
        style.configure("TCheckbutton", background=CARD)
        style.configure("Treeview", background=CARD, foreground=INK, fieldbackground=CARD,
                        rowheight=25)
        style.map("Treeview", background=[("selected", "#31536c")])
        style.configure("TNotebook.Tab", padding=(12, 8))
        style.map("TNotebook.Tab", background=[("selected", CARD)])

    def build(self):
        header = ttk.Frame(self.root, padding=(18, 12))
        header.pack(fill="x")
        ttk.Label(header, text="PanelKit Studio", style="Brand.TLabel").pack(side="left")
        ttk.Button(header, text="Export to Arduino…", style="Accent.TButton",
                   command=self.export).pack(side="right")
        for label, callback in reversed((("New", self.new), ("Open…", self.open),
                                        ("Save", self.save), ("Undo", self.undo), ("Redo", self.redo))):
            ttk.Button(header, text=label, command=callback).pack(side="right", padx=(0,5))
        panes = ttk.Panedwindow(self.root, orient="horizontal")
        panes.pack(fill="both", expand=True, padx=14, pady=(0,8))
        left = ttk.Frame(panes, width=217, padding=10, style="Card.TFrame")
        middle = ttk.Frame(panes, width=555, padding=9, style="Card.TFrame")
        right = ttk.Frame(panes, width=325, padding=10, style="Card.TFrame")
        panes.add(left, weight=0)
        panes.add(middle, weight=1)
        panes.add(right, weight=0)

        ttk.Label(left, text="Widgets", style="Title.TLabel").pack(anchor="w", pady=(0,6))
        self.palette = ttk.Combobox(left, values=tuple(PALETTE), state="readonly", width=17)
        self.palette.set(next(label for label, kind in PALETTE.items() if kind == "Button"))
        self.palette.pack(fill="x")
        ttk.Button(left, text="Add widget", command=self.add).pack(fill="x", pady=(5,12))
        ttk.Label(left, text="Object tree", style="Title.TLabel").pack(anchor="w", pady=(0,6))
        tree_frame = ttk.Frame(left, style="Card.TFrame")
        tree_frame.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(tree_frame, show="tree", selectmode="browse")
        scroll = ttk.Scrollbar(tree_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.tree.pack(side="left", fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", self.tree_select)
        for label, callback in (("Duplicate", self.duplicate), ("Remove", self.remove)):
            ttk.Button(left, text=label, command=callback).pack(fill="x", pady=(5,0))

        tools = ttk.Frame(middle, style="Card.TFrame")
        tools.pack(fill="x", pady=(0,7))
        ttk.Label(tools, text="Design canvas", style="Title.TLabel").pack(side="left")
        selector = ttk.Combobox(tools, values=ZOOMS, textvariable=self.zoom, state="readonly", width=7)
        selector.pack(side="right")
        selector.bind("<<ComboboxSelected>>", lambda _: self.paint())
        self.snap = tk.BooleanVar(value=True)
        ttk.Checkbutton(tools, text="Snap to 4 px", variable=self.snap).pack(side="right", padx=8)
        field = ttk.Frame(middle, style="Card.TFrame")
        field.pack(fill="both", expand=True)
        field.columnconfigure(0, weight=1)
        field.rowconfigure(0, weight=1)
        self.canvas = tk.Canvas(field, background="#0c141e", highlightthickness=0)
        self.canvas.grid(row=0,column=0,sticky="nsew")
        v = ttk.Scrollbar(field, command=self.canvas.yview)
        h = ttk.Scrollbar(field, orient="horizontal", command=self.canvas.xview)
        v.grid(row=0,column=1,sticky="ns")
        h.grid(row=1,column=0,sticky="ew")
        self.canvas.configure(yscrollcommand=v.set, xscrollcommand=h.set)
        self.preview = Preview(self.canvas)
        self.canvas.bind("<Button-1>", self.pointer_down)
        self.canvas.bind("<B1-Motion>", self.pointer_move)
        self.canvas.bind("<ButtonRelease-1>", self.pointer_up)
        self.canvas.bind("<Configure>", lambda _: self.paint())
        self.canvas.bind("<MouseWheel>", lambda e: self.canvas.yview_scroll(-int(e.delta/120), "units"))

        self.inspector_title = ttk.Label(right, text="Screen", style="Title.TLabel")
        self.inspector_title.pack(anchor="w", pady=(0,8))
        self.inspector = ttk.Notebook(right)
        self.inspector.pack(fill="both", expand=True)
        self.property_frame = self.scrolling_tab("Properties")
        self.event_frame = self.scrolling_tab("Events & code")
        ttk.Button(right, text="Apply changes", style="Accent.TButton",
                   command=self.apply).pack(fill="x", pady=(8,0))
        ttk.Label(self.root, textvariable=self.status, foreground=DIM,
                  padding=(16, 4, 16, 8)).pack(fill="x")

        menu = tk.Menu(self.root, tearoff=False)
        file_menu = tk.Menu(menu, tearoff=False)
        for label, command in (("New", self.new), ("Open…", self.open), ("Save", self.save),
                               ("Save as…", lambda: self.save(as_new=True)), ("Open demo", self.demo),
                               ("Preview generated C++", self.preview_code), ("Export to Arduino…", self.export),
                               ("Export SD menu page…", self.export_sd_menu),
                               ("Export full SD view…", self.export_sd_view)):
            file_menu.add_command(label=label, command=command)
        menu.add_cascade(label="File", menu=file_menu)
        edit = tk.Menu(menu, tearoff=False)
        for label, command in (("Undo", self.undo), ("Redo", self.redo),
                               ("Clear history", self.clear_history),
                               ("Duplicate widget", self.duplicate), ("Remove widget", self.remove)):
            edit.add_command(label=label, command=command)
        menu.add_cascade(label="Edit", menu=edit)
        self.root.config(menu=menu)
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        for key, handler in (("z", self.undo), ("y", self.redo)):
            self.root.bind(f"<Control-{key}>", lambda _, fn=handler: self.history_key(fn))
        for key, handler in (("s", self.save), ("o", self.open), ("n", self.new)):
            self.root.bind(f"<Control-{key}>", lambda _, fn=handler: fn())

    def scrolling_tab(self, label):
        page = ttk.Frame(self.inspector, style="Card.TFrame")
        self.inspector.add(page, text=label)
        scroll = ttk.Scrollbar(page, orient="vertical")
        scroll.pack(side="right", fill="y")
        view = tk.Canvas(page, background=CARD, highlightthickness=0, width=310,
                         yscrollcommand=scroll.set)
        view.pack(fill="both", expand=True)
        scroll.configure(command=view.yview)
        frame = ttk.Frame(view, padding=(8,8,8,4), style="Card.TFrame")
        window = view.create_window((0,0), window=frame, anchor="nw")
        frame.bind("<Configure>", lambda _: view.configure(scrollregion=view.bbox("all")))
        view.bind("<Configure>", lambda event: view.itemconfigure(window, width=event.width))
        return frame

    def title(self):
        name = self.project.path.stem if self.project.path else "Untitled"
        dirty = self.project.to_dict() != self.saved
        self.root.title(f"{name}{' *' if dirty else ''} · PanelKit Studio")

    def paint(self):
        self.preview.draw(self.project, self.selection, int(self.zoom.get()[:-1])/100)

    def refresh(self):
        self.building = True
        try:
            self.tree.delete(*self.tree.get_children())
            self.tree.insert("", "end", "screen_node", text="Screen", open=True)
            for widget in self.project.ordered():
                parent = "widget:" + widget.parent if widget.parent else "screen_node"
                self.tree.insert(parent, "end", "widget:" + widget.name,
                                 text=f"{ICONS[widget.kind]}  {widget.name} · {widget.kind}", open=True)
            self.select(self.selection, True)
            self.title()
        finally:
            self.building = False

    def select(self, name, force=False):
        if self.project.widget(name) is None:
            name = ""
        if name == self.selection and not force:
            return
        if not force and self.form and not self.apply():
            node = "widget:" + self.selection if self.selection else "screen_node"
            if self.tree.selection() != (node,):
                self.tree.selection_set(node)
            return
        self.selection = name
        node = "widget:" + name if name else "screen_node"
        if self.tree.exists(node) and self.tree.selection() != (node,):
            self.tree.selection_set(node)
            self.tree.see(node)
        self.inspector_form()
        self.paint()

    def tree_select(self, _):
        if not self.building and self.tree.selection():
            node = self.tree.selection()[0]
            self.select(node.split(":",1)[1] if ":" in node else "")

    def row(self, frame, key, value, default=None, kind="", choices=None):
        label = key.replace("_"," ").capitalize()
        row = ttk.Frame(frame, style="Card.TFrame")
        row.pack(fill="x", pady=(2,5))
        if type(value) is bool:
            control = tk.BooleanVar(value=value)
            ttk.Checkbutton(row,text=label,variable=control).pack(anchor="w")
        else:
            ttk.Label(row,text=label,style="Card.TLabel").pack(anchor="w",pady=(0,2))
            line = ttk.Frame(row,style="Card.TFrame")
            line.pack(fill="x")
            options = choices if choices is not None else kind.split() if " " in kind else None
            if options is not None:
                control = tk.StringVar(value=value)
                ttk.Combobox(line,textvariable=control,values=options,state="readonly").pack(fill="x")
            elif key == "selected":
                box = ttk.Combobox(line,state="readonly")
                box.pack(fill="x")
                self.selected_choice = box
                self.refresh_selection_choices(value)
                control = Reader(lambda: max(0,box.current()))
            elif isinstance(value,(dict,list)):
                control = RecordValue(value)
                summary = tk.StringVar()

                def update(result):
                    origins = None
                    if key == "columns" and isinstance(result,tuple):
                        result,origins = result
                    control.value = result
                    summary.set("Default" if not result else f"{len(result)} configured")
                    if origins is not None and "rows" in self.form:
                        records = self.form["rows"][0]
                        records.value = [[cells[i] if i is not None and i < len(cells) else ""
                                          for i in origins] for cells in records.value] if origins else []
                    if key in ("items","pages"):
                        self.refresh_selection_choices()
                update(value)
                ttk.Label(line,textvariable=summary,style="Hint.TLabel").pack(side="left",fill="x",expand=True)
                ttk.Button(line,text="Edit…",command=lambda: self.edit_collection(
                    key,control,update)).pack(side="left")
            elif type(default if default is not None else value) in (float,int):
                fallback = default if default is not None else value
                bounds = NUMBERS.get(key,(-32768,32767) if key == "value" else (0,32767))
                if type(fallback) is float:
                    bounds = NUMBERS.get(key,(0.0,1.0,0.05) if key == "value" and
                                         self.project.widget(self.selection).kind == "Progress"
                                         else (-1000000.0,1000000.0,0.1))
                editor,get = numbers(line,value,*bounds)
                editor.pack(fill="x")
                control = Reader(get)
            else:
                control = tk.StringVar(value=value)
                ttk.Entry(line,textvariable=control,state="readonly" if kind in ("image","font","color")
                          else "normal").pack(side="left",fill="x",expand=True)
                if kind in ("image","font"):
                    types = [("Headers","*.h *.hpp")] if kind == "font" else [
                        ("Images","*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tif *.tiff *.ppm"),("All files","*.*")]
                    ttk.Button(line,text="Browse…",command=lambda: self.pick_file(control,types,kind)
                               ).pack(side="left",padx=(4,0))
                    ttk.Button(line,text="Clear",command=lambda: self.clear_file(control,kind)
                               ).pack(side="left",padx=(4,0))
                if kind == "color":
                    ttk.Button(line,text="Pick…",command=lambda: self.pick_color(control)).pack(side="left",padx=4)
        self.form[key] = (control, None)

    def refresh_selection_choices(self, index=None):
        box = self.selected_choice
        if box is None:
            return
        index = box.current() if index is None else index
        key = "pages" if "pages" in self.form else "items"
        source = self.form.get(key)
        items = source[0].get() if source else []
        labels = [item[0] if key == "pages" else item for item in items]
        box.configure(values=[f"{i + 1}. {label}" for i,label in enumerate(labels)] or ["No choices"])
        box.current(min(max(0,index), max(0,len(labels)-1)))

    def edit_collection(self, key, control, update):
        widget = self.project.widget(self.selection)
        children = []
        if key == "pages":
            children = [item.name for item in self.project.widgets if item.parent == widget.parent
                        and WIDGETS[item.kind].container]
        elif key == "layout":
            children = [item.name for item in self.project.widgets if item.parent == widget.name]
        columns = self.form["columns"][0].get() if key == "rows" else []
        edit_record(self.root,key,control.get(),update,widget=widget,children=children,columns=columns)

    def pick_file(self, field, types, kind):
        chosen = filedialog.askopenfilename(parent=self.root, filetypes=types)
        if chosen:
            field.set(chosen)
            if kind == "font":
                try:
                    source = Path(chosen).read_text(encoding="utf-8-sig")
                    namespace = re.search(r"\bnamespace\s+([A-Za-z_]\w*)\s*\{",source)
                    symbol = f"{namespace[1]}::Font" if namespace else ""
                    self.form["font_symbol"][0].set(symbol)
                except (OSError,ValueError) as error:
                    self.error("Could not read header",error)

    def clear_file(self,field,kind):
        field.set("")
        if kind == "font":
            self.form["font_symbol"][0].set("")

    def choose_typeface(self):
        if not self.apply():
            return
        try:
            require_glyph_bridge()
        except ImportError:
            self.error("Glyph Bridge required",
                       "Install the updated Glyph Bridge checkout in the same Python environment "
                       "to browse typefaces. The bundled Inter font and bitmap header import "
                       "are available now. See the Studio setup guide for the install command.")
            return
        TypefaceDialog(self.root,self.set_typeface)

    def set_typeface(self, data):
        if data == self.project.font_embedded:
            return
        project = copy.deepcopy(self.project)
        project.font_embedded = data
        project.font_header = project.font_symbol = ""
        self.change(project,"")

    def pick_color(self, field):
        _, color = colorchooser.askcolor(field.get(), parent=self.root)
        if color:
            field.set(color)

    def code_row(self, key, title, value, height=7, signature=None):
        ttk.Label(self.event_frame,text=title,style="Card.TLabel").pack(anchor="w",pady=(4,3))
        editor = tk.Text(self.event_frame,height=height,wrap="none",bg=FIELD,fg=INK,
                         insertbackground=MINT,relief="flat",font=("Consolas",10))
        editor.pack(fill="x",pady=(0,8))
        editor.insert("1.0",value)
        self.form[key] = (editor, "")
        for shortcut, handler in (("z", self.undo), ("y", self.redo)):
            editor.bind(f"<Control-{shortcut}>",lambda _, fn=handler: self.history_key(fn))
        if signature is not None:
            ttk.Button(self.event_frame, text="Add widget action…",
                       command=lambda: self.widget_action(editor, signature)).pack(anchor="w", pady=(0,8))

    def widget_action(self, editor, signature):
        window = tk.Toplevel(self.root)
        window.title("Add widget action")
        window.transient(self.root)
        window.grab_set()
        content = ttk.Frame(window,style="Card.TFrame",padding=14)
        content.pack(fill="both",expand=True)
        arguments = {name: kind for part in signature.split(",") if part.strip()
                     for kind,name in [part.strip().rsplit(" ",1)]}
        target = ttk.Combobox(content,state="readonly",values=[w.name for w in self.project.widgets])
        method = ttk.Combobox(content,state="readonly")
        source = ttk.Combobox(content,state="readonly")
        for label,control in (("Target widget",target),("Action",method),("Value source",source)):
            ttk.Label(content,text=label,style="Card.TLabel").pack(anchor="w")
            control.pack(fill="x",pady=(0,8))
        field = ttk.Frame(content,style="Card.TFrame")
        field.pack(fill="x")
        candidates, argument = {}, None

        def update_method(_=None):
            nonlocal argument
            for child in field.winfo_children():
                child.destroy()
            argument = None
            default = candidates.get(method.get())
            types = {bool:{"bool"}, int:{"int","float"}, float:{"int","float"}, str:{"const char*"}}
            same = ["Event: " + name for name,kind in arguments.items()
                    if kind in types.get(type(default), set())]
            source.configure(values=["Constant", *same],state="readonly" if default is not None else "disabled")
            source.set("Constant" if default is not None else "")
            if type(default) is bool:
                var = tk.BooleanVar(value=default)
                widget = ttk.Checkbutton(field,text="Enabled",variable=var)
                argument = var.get
            elif type(default) in (int,float):
                bounds = NUMBERS.get("font_scale", (0.0,16.0,0.05)) if method.get() == "setFontScale" else (
                    (-1000000.0,1000000.0,0.1) if type(default) is float else (-32768,32767))
                widget, argument = numbers(field,default,*bounds)
            elif isinstance(default,str):
                widget = ttk.Entry(field)
                widget.insert(0,default)
                argument = widget.get
            if argument:
                widget.pack(fill="x")

        def update_target(_=None):
            candidates.clear()
            widget = self.project.widget(target.get())
            if not widget:
                return
            candidates.update(setVisible=True, setOrder=0, activate=None, deactivate=None, invalidate=None)
            candidates.update({field.method: field.default for field in WIDGETS[widget.kind].fields.values()
                               if field.method and type(field.default) in (bool,int,float,str)
                               and field.kind != "color" and " " not in field.kind})
            candidates.update({"Plot":{"append":0.0,"clearSamples":None},"Window":{"close":None},
                               "Flex":{"relayout":None}}.get(widget.kind,{}))
            method.configure(values=sorted(candidates))
            method.set("setVisible")
            update_method()

        def insert():
            try:
                value = source.get()[7:] if source.get().startswith("Event: ") else (
                    cpp(argument()) if argument else "")
            except ValueError as error:
                messagebox.showerror("Invalid value",str(error), parent=window)
                return
            if editor.get("1.0","end-1c").strip():
                editor.insert("end-1c","\n")
            editor.insert("end-1c",f"widgets::{target.get()}.{method.get()}({value});")
            window.destroy()

        target.bind("<<ComboboxSelected>>",update_target)
        method.bind("<<ComboboxSelected>>",update_method)
        target.current(0)
        update_target()
        ttk.Button(content,text="Insert action",command=insert,style="Accent.TButton").pack(pady=(10,0))

    def inspector_form(self):
        for frame in (self.property_frame,self.event_frame):
            for child in frame.winfo_children():
                child.destroy()
        self.form.clear()
        self.selected_choice = None
        widget = self.project.widget(self.selection)
        frame = self.property_frame
        if not widget:
            self.inspector_title.configure(text="Screen")
            face = self.project.font_embedded.get("label", "Custom bitmap header" if self.project.font_header else "Bundled Inter")
            ttk.Label(frame,text=face,style="Title.TLabel",wraplength=285).pack(anchor="w")
            ttk.Button(frame,text="Choose typeface…",command=self.choose_typeface).pack(fill="x",pady=4)
            if self.project.font_embedded:
                ttk.Button(frame,text="Use bundled Inter",command=lambda: self.set_typeface({})).pack(fill="x")
            for key in ("width", "height", "background", "buffered", "theme",
                        "font_header", "font_symbol", "pins"):
                kind = {"background":"color","font_header":"font"}.get(key,"")
                self.row(frame,key,getattr(self.project,key),kind=kind)
            ttk.Separator(frame).pack(fill="x",pady=7)
            ttk.Label(frame,text="Input devices",style="Title.TLabel").pack(anchor="w")
            options = {"serial_port":SERIAL_PORTS, "serial_format":SERIAL_FORMATS}
            for key in ("touch_enabled","drag_threshold","double_click_ms","deferred_clicks",
                        "serial_port","serial_format","serial_baud","serial_budget"):
                self.row(frame,key,getattr(self.project,key),choices=options.get(key))
            ttk.Label(frame,text="Touch and serial can run together. Text sends bytes to the focused widget. Commands accepts newline-delimited "
                      "pointer, wheel and keyboard commands. Checked commands requires a CRC-8 prefix.",wraplength=260,style="Hint.TLabel").pack(anchor="w")
            for key, label in (("includes","Additional includes and declarations"),
                               ("configure_cpp","Run once after configuring widgets"),
                               ("frame_cpp","Run before each frame")):
                self.code_row(key,label,getattr(self.project,key))
            return
        self.inspector_title.configure(text=f"{widget.name}  ·  {widget.kind}")
        schema = WIDGETS[widget.kind]
        parent_choices = [""] + [other.name for other in self.project.widgets
                                 if other.name != widget.name and WIDGETS[other.kind].container]
        for key in ("name","parent","x","y","width","height","order","visible","active"):
            self.row(frame,key,getattr(widget,key),choices=parent_choices if key == "parent" else None)
        ttk.Separator(frame).pack(fill="x",pady=7)
        for key, value in values(widget).items():
            field = schema.fields[key]
            self.row(frame,key,value,field.default,field.kind)
        if schema.events:
            for event, signature in schema.events.items():
                self.code_row(event,f"{event}({signature})",widget.events.get(event,""),4,signature)
            ttk.Label(self.event_frame,text="Enter the C++ callback body. Access any widget as "
                      "widgets::object_id. onInput receives the event type, source, coordinates and data.",
                      wraplength=260,style="Hint.TLabel").pack(anchor="w")
        else:
            ttk.Label(self.event_frame,text="This widget has no callback properties. "
                      "Use Screen → Events & code for runtime logic.",
                      wraplength=260,style="Hint.TLabel").pack(anchor="w")

    def apply(self):
        candidate = copy.deepcopy(self.project)
        widget = candidate.widget(self.selection)
        try:
            for key, (control, default) in self.form.items():
                value = control.get("1.0","end-1c") if isinstance(control,tk.Text) else control.get()
                if not widget:
                    setattr(candidate,key,value)
                elif key in WIDGETS[widget.kind].fields:
                    widget.properties[key] = value
                elif key in WIDGETS[widget.kind].events:
                    if value.strip():
                        widget.events[key] = value
                    else:
                        widget.events.pop(key,None)
                else:
                    setattr(widget,key,value)
            if widget and widget.name != self.selection:
                for other in candidate.widgets:
                    if other.parent == self.selection:
                        other.parent = widget.name
                    for field in ("pages","layout"):
                        for entry in other.properties.get(field, []):
                            index = 1 if field == "pages" else 0
                            if entry[index] == self.selection:
                                entry[index] = widget.name
            if candidate.font_header:
                candidate.font_embedded = {}
            validate(candidate)
        except (ValueError,TypeError,IndexError) as error:
            self.error("Check the properties",error)
            return False
        if candidate.to_dict() != self.project.to_dict():
            self.change(candidate, widget.name if widget else "")
        self.status.set("Properties applied")
        return True

    def change(self, project, selection):
        self.history.append((self.project,self.selection))
        self.history = self.history[-60:]
        self.redo_history.clear()
        self.project, self.selection = project, selection
        self.refresh()

    def history_key(self, action):
        action()
        return "break"

    def clear_history(self):
        if self.apply():
            self.history.clear()
            self.redo_history.clear()
            self.status.set("Session history cleared.")

    def undo(self):
        if self.apply() and self.history:
            self.redo_history.append((self.project,self.selection))
            self.project,self.selection = self.history.pop()
            self.refresh()

    def redo(self):
        if self.apply() and self.redo_history:
            self.history.append((self.project,self.selection))
            self.project,self.selection = self.redo_history.pop()
            self.refresh()

    def unique_name(self, project, base):
        index = 1
        while project.widget(f"{base}{index}"):
            index += 1
        return f"{base}{index}"

    def add(self):
        if not self.apply():
            return
        kind = PALETTE.get(self.palette.get(), self.palette.get())
        project = copy.deepcopy(self.project)
        selected = project.widget(self.selection)
        parent = selected.name if selected and WIDGETS[selected.kind].container else (
            selected.parent if selected else "")
        name = self.unique_name(project,re.sub(r"\W","",kind).lower())
        widget = Widget.create(kind,name,parent=parent)
        if selected and selected.parent == parent:
            widget.x, widget.y = selected.x+10, selected.y+10
        project.widgets.append(widget)
        self.change(project,name)
        self.status.set("Drag the widget to position it, or use Properties for exact geometry.")

    def duplicate(self):
        if not self.selection or not self.apply():
            return
        project = copy.deepcopy(self.project)
        clone = copy.deepcopy(project.widget(self.selection))
        clone.name = self.unique_name(project,clone.name + "_copy")
        clone.x += 8
        clone.y += 8
        if clone.kind == "Flex":
            clone.properties["layout"] = []
        if clone.kind == "Tabs":
            clone.properties["pages"] = []
        project.widgets.append(clone)
        try:
            validate(project)
        except ValueError as error:
            self.error("Could not duplicate widget",error)
            return
        self.change(project,clone.name)

    def remove(self):
        if not self.selection:
            return
        target = self.selection
        project = copy.deepcopy(self.project)
        removed = {target}
        for widget in project.ordered():
            if widget.parent in removed:
                removed.add(widget.name)
        project.widgets = [widget for widget in project.widgets if widget.name not in removed]
        for widget in project.widgets:
            for key,index in (("pages",1),("layout",0)):
                if key in widget.properties:
                    widget.properties[key] = [entry for entry in widget.properties[key]
                                              if entry[index] not in removed]
        self.change(project,"")

    def point(self, event):
        x,y = self.canvas.canvasx(event.x),self.canvas.canvasy(event.y)
        return ((x-self.preview.origin[0])/self.preview.scale,
                (y-self.preview.origin[1])/self.preview.scale)

    def pointer_down(self,event):
        if not self.apply():
            return
        x,y = self.point(event)
        current = self.canvas.find_withtag("current")
        resize = bool(current and "resize" in self.canvas.gettags(current[0]))
        if not resize:
            name = next((name for name,(a,b,w,h) in reversed(list(self.preview.hit_boxes.items()))
                         if a <= x < a+w and b <= y < b+h),"")
            self.select(name)
        selected = self.project.widget(self.selection)
        if selected:
            self.gesture = (x,y,selected.x,selected.y,selected.width,selected.height,resize,
                            copy.deepcopy(self.project))
            parent = self.project.widget(selected.parent)
            if parent and (parent.kind == "Flex" or
                           values(parent).get("grid_columns") or
                           (parent.kind == "Window" and values(parent)["fit_single_child"])):
                self.gesture = None
                self.status.set("The parent layout controls this widget’s geometry.")

    def pointer_move(self,event):
        if not self.gesture:
            return
        x,y = self.point(event)
        a,b,sx,sy,w,h,resize,_ = self.gesture
        widget = self.project.widget(self.selection)
        grid = 4 if self.snap.get() else 1
        dx,dy = round((x-a)/grid)*grid,round((y-b)/grid)*grid
        if resize:
            widget.width,widget.height = max(1,min(4096,w+dx)),max(1,min(4096,h+dy))
        else:
            widget.x,widget.y = max(-32768,min(32767,sx+dx)),max(-32768,min(32767,sy+dy))
        self.paint()

    def pointer_up(self,_):
        if not self.gesture:
            return
        before = self.gesture[-1]
        self.gesture = None
        if before != self.project:
            self.history.append((before,self.selection))
            self.history = self.history[-60:]
            self.redo_history.clear()
            self.inspector_form()
            self.title()

    def error(self, title, error):
        messagebox.showerror(title,str(error),parent=self.root)

    def can_close(self):
        if not self.apply():
            return False
        if self.project.to_dict() == self.saved:
            return True
        choice = messagebox.askyesnocancel("Save project","Save changes to the current project?",
                                           parent=self.root)
        return self.save() if choice else choice is False

    def set_project(self, project, dirty=False):
        self.project,self.selection = project,""
        self.saved = {} if dirty else project.to_dict()
        self.history.clear()
        self.redo_history.clear()
        self.refresh()

    def new(self):
        if self.can_close():
            self.set_project(Project())

    def demo(self):
        if self.can_close():
            from .demo import demo_project
            self.set_project(demo_project(),dirty=True)

    def open(self):
        if not self.can_close():
            return
        name = filedialog.askopenfilename(filetypes=[("PanelKit project","*.panelkit.json"),("JSON","*.json")],
                                          parent=self.root)
        if name:
            try:
                self.set_project(load_project(name))
            except (OSError,ValueError,TypeError) as error:
                self.error("Could not open the project",error)

    @staticmethod
    def rebase_assets(source, path):
        project = copy.deepcopy(source)
        assets = [(project,"font_header",project.font_header)]
        assets.extend((widget.properties,"image",widget.properties.get("image",""))
                      for widget in project.widgets)
        for owner,key,original in assets:
            if not original:
                continue
            absolute = project.asset(original)
            try:
                rebased = os.path.relpath(absolute,path.parent)
            except ValueError:
                rebased = str(absolute)
            if isinstance(owner,dict):
                owner[key] = rebased
            else:
                setattr(owner,key,rebased)
        project.path = path
        return project

    def save(self, as_new=False):
        if not self.apply():
            return False
        path = self.project.path
        if not path or as_new:
            chosen = filedialog.asksaveasfilename(parent=self.root,
                defaultextension=".panelkit.json",filetypes=[("PanelKit project","*.panelkit.json")])
            if not chosen:
                return False
            path = Path(chosen)
        try:
            path = Path(path).resolve()
            project = self.rebase_assets(self.project,path)
            save_project(project,path)
        except (OSError,ValueError) as error:
            self.error("Could not save the project",error)
            return False
        if self.project.path != path:
            self.history = [(self.rebase_assets(old,path),selection) for old,selection in self.history]
            self.redo_history = [(self.rebase_assets(old,path),selection) for old,selection in self.redo_history]
        self.project = project
        self.saved = project.to_dict()
        self.refresh()
        self.status.set(f"Saved {path.name}")
        return True

    def preview_code(self):
        if not self.apply():
            return
        from .export import Exporter
        try:
            code = Exporter(self.project).render()[SOURCE]
        except (OSError,ValueError) as error:
            self.error("Could not generate C++",error)
            return
        view = tk.Toplevel(self.root)
        view.title(SOURCE)
        editor = tk.Text(view,width=100,height=32,bg=FIELD,fg=INK,font=("Consolas",10))
        editor.insert("1.0",code)
        editor.configure(state="disabled")
        editor.pack(fill="both",expand=True)

    def export_sd_view(self):
        if not self.apply():
            return
        name = simpledialog.askstring("Full SD view", "Firmware view ID", parent=self.root,
                                      initialvalue="controls")
        if not name:
            return
        title = simpledialog.askstring("Full SD view", "Menu page title", parent=self.root,
                                       initialvalue=name.replace("_", " ").title())
        if not title:
            return
        target = filedialog.askdirectory(parent=self.root, title="Select output folder for C++ and PKM1")
        if not target:
            return
        try:
            outputs = export_menu_view(self.project, target, name, title)
        except (OSError, ValueError, TypeError) as error:
            self.error("Could not export full SD view", error)
            return
        self.status.set(f"Exported {len(outputs)} SD view files to {target}")

    def export_sd_menu(self):
        if not self.apply():
            return
        selected = self.project.widget(self.selection)
        section = ""
        if selected:
            section = selected.name if WIDGETS[selected.kind].container else selected.parent
        title = simpledialog.askstring("SD menu page", "Page title", parent=self.root,
                                       initialvalue=section or "Menu")
        if not title:
            return
        target = filedialog.asksaveasfilename(parent=self.root, defaultextension=".pkm",
                                             initialfile=(section or "index") + ".pkm",
                                             filetypes=[("PanelKit menu page", "*.pkm")])
        if not target:
            return
        try:
            saved = export_menu_page(self.project, target, title, section)
        except (OSError, ValueError, TypeError) as error:
            self.error("Could not export SD menu page", error)
            return
        self.status.set(f"Exported SD menu page to {saved}")

    def export(self):
        if not self.apply():
            return
        action = messagebox.askyesnocancel("Export to Arduino",
            "Create a new Arduino sketch?\n\nYes creates a starter sketch. "
            "No adds the generated files to an existing sketch folder.",parent=self.root)
        if action is None:
            return
        folder = filedialog.askdirectory(parent=self.root,title="Choose the sketch folder" if not action
                                           else "Choose where to create the sketch")
        if not folder:
            return
        target = Path(folder)
        if action:
            name = simpledialog.askstring("New Arduino sketch","Sketch name:",parent=self.root,
                                          initialvalue="PanelKitSketch")
            if not name:
                return
            target = target / name
        try:
            files = export_project(self.project,target,create_sketch=action)
        except (OSError,ValueError,TypeError) as error:
            self.error("Arduino export failed",error)
            return
        self.last_export = target
        self.status.set(f"Exported {len(files)} files to {target}")
        self.export_summary(target, action)

    def open_location(self, target):
        try:
            if sys.platform == "win32":
                os.startfile(target)
            else:
                subprocess.Popen(["open" if sys.platform == "darwin" else "xdg-open",str(target)])
        except OSError as error:
            self.error("Could not open the exported file",error)

    def export_summary(self, target, new_sketch):
        window = tk.Toplevel(self.root)
        window.title("Arduino export complete")
        window.transient(self.root)
        content = ttk.Frame(window, style="Card.TFrame", padding=16)
        content.pack(fill="both", expand=True)
        ttk.Label(content, text="Sketch files exported", style="Title.TLabel").pack(anchor="w")
        ttk.Label(content, text=str(target), wraplength=560,
                  style="Hint.TLabel").pack(anchor="w", pady=(6,10))
        buttons = ttk.Frame(content, style="Card.TFrame")
        code = ('#include "PanelKitGenerated.h"\n\n'
                '// After initializing your display in setup():\n'
                'panelkit_ui::configure();\n\n'
                '// Optional in setup() to initialize the configured serial port:\n'
                'panelkit_ui::beginInputs();\n\n'
                '// In loop() with a TeensyTFT-compatible display:\n'
                'panelkit_ui::pollTouch(display);\n'
                'panelkit_ui::pollInputs();\n'
                'panelkit_ui::update();\n'
                'panelkit_ui::present(display);')
        if new_sketch:
            sketch = target / (target.name + ".ino")
            ttk.Button(buttons, text="Open sketch", style="Accent.TButton",
                       command=lambda: self.open_location(sketch)).pack(side="left")
        else:
            editor = tk.Text(content, width=64, height=9, bg=FIELD, fg=INK,
                             font=("Consolas",10), relief="flat")
            editor.pack(fill="both", expand=True, pady=(0,10))
            editor.insert("1.0",code)
            editor.configure(state="disabled")

            def copy():
                self.root.clipboard_clear()
                self.root.clipboard_append(code)
                self.status.set("Copied the Arduino integration snippet.")
            ttk.Button(buttons,text="Copy integration snippet",
                       command=copy,style="Accent.TButton").pack(side="left")
        buttons.pack(fill="x")
        ttk.Button(buttons,text="Open folder",command=lambda: self.open_location(target)
                   ).pack(side="left", padx=6)
        ttk.Button(buttons,text="Done",command=window.destroy).pack(side="right")

    def close(self):
        if self.can_close():
            self.history.clear()
            self.redo_history.clear()
            self.root.destroy()


def run(path=None):
    project = load_project(path) if path else Project()
    root = tk.Tk()
    Studio(root,project)
    root.mainloop()

