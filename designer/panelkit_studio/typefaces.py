"""Load native typefaces with Glyph Bridge and retain their generated bitmap."""

from functools import lru_cache
from pathlib import Path
from queue import Empty, Queue
import re
from tempfile import TemporaryDirectory
from threading import Thread
import tkinter as tk
from tkinter import filedialog, ttk

from PIL import Image, ImageTk

from .properties import numbers

INSTALLED = "Installed fonts"
GOOGLE = "Google Fonts"
FONT_FILE = "Font file"


def require_glyph_bridge():
    """Check the font browser API, including when using an editable checkout."""
    from glyph_bridge import core, gui

    for module, names in (
        (core, ("LICENSE_FILES", "export_header")),
        (gui, ("GOOGLE_SOURCE", "LOCAL_SOURCE", "bitmap_preview", "font_options",
               "google_catalog", "local_catalog")),
    ):
        if any(not hasattr(module, name) for name in names):
            raise ImportError("The installed Glyph Bridge lacks Studio's font browser API.")


def rasterize(selection, size):
    from glyph_bridge.core import LICENSE_FILES, export_header

    with TemporaryDirectory(prefix="panelkit-typeface-") as scratch:
        header = Path(export_header(selection.path, selection.family, selection.style,
                                    size, selection.synthetic, scratch))
        source = header.read_text(encoding="ascii")
    label = f"{selection.family} · {selection.style} · {size} px"
    licenses = sorted(file for file in selection.path.parent.iterdir()
                      if file.is_file() and file.name.lower() in LICENSE_FILES)
    license_text = "\n".join(file.read_text(encoding="utf-8-sig", errors="replace") for file in licenses)
    return {"label": label, "header": source, "symbol": header.stem + "::Font", "license": license_text}


class TypefaceDialog:
    def __init__(self, parent, apply):
        self.apply = apply
        self.jobs = Queue()
        self.generation = 0
        self.fonts = {}
        self.styles = {}
        self.photo = None
        self.window = tk.Toplevel(parent)
        self.window.title("Choose a typeface")
        self.window.transient(parent)
        self.window.grab_set()
        body = ttk.Frame(self.window,style="Card.TFrame",padding=16)
        body.pack(fill="both",expand=True)
        self.source = ttk.Combobox(body,values=(INSTALLED,GOOGLE,FONT_FILE),state="readonly",width=35)
        self.source.set(INSTALLED)
        self.family = ttk.Combobox(body,state="readonly",width=35)
        self.style = ttk.Combobox(body,state="readonly",width=35)
        self.size, self.read_size = numbers(body,16,6,64)
        self.pick = ttk.Button(body,text="Choose TTF or OTF…",command=self.font_file)
        self.sample = tk.Label(body, text="Bitmap sample", bg="#111923", fg="#82e7cd",
                               height=4, padx=12, pady=10)
        for index,(name,control) in enumerate((("Source",self.source),("Family",self.family),
                                               ("Style",self.style),("Raster size",self.size))):
            ttk.Label(body,text=name,style="Card.TLabel").grid(row=index,column=0,sticky="w",padx=(0,10))
            control.grid(row=index,column=1,sticky="ew",pady=4)
        self.search = tk.StringVar()
        self.filter_box = ttk.Entry(body,textvariable=self.search)
        self.filter_box.grid(row=4,column=1,sticky="ew",pady=4)
        ttk.Label(body,text="Filter families",style="Card.TLabel").grid(row=4,column=0,sticky="w")
        self.search.trace_add("write",lambda *_: self.filter_families())
        self.status = tk.StringVar(value="Loading fonts…")
        ttk.Label(body,textvariable=self.status,wraplength=420,style="Hint.TLabel").grid(
            row=5,column=0,columnspan=2,sticky="w",pady=6)
        self.sample.grid(row=6,column=0,columnspan=2,sticky="ew")
        buttons = ttk.Frame(body,style="Card.TFrame")
        buttons.grid(row=7,column=0,columnspan=2,sticky="ew",pady=(10,0))
        self.select = ttk.Button(buttons,text="Use typeface",style="Accent.TButton",
                                 command=self.accept,state="disabled")
        self.select.pack(side="right")
        ttk.Button(buttons,text="Cancel",command=self.window.destroy).pack(side="right",padx=8)
        self.source.bind("<<ComboboxSelected>>",self.load)
        self.family.bind("<<ComboboxSelected>>",self.load_styles)
        self.style.bind("<<ComboboxSelected>>",self.preview)
        self.size.bind("<Return>",self.preview)
        self.size.bind("<FocusOut>",self.preview)
        self.window.bind("<Escape>",lambda _: self.window.destroy())
        self.window.after(50,self.poll)
        self.load()

    def run(self, operation, callback, status):
        self.generation += 1
        generation = self.generation
        self.status.set(status)
        self.select.configure(state="disabled")

        def worker():
            try:
                result = operation()
            except Exception as error:
                self.jobs.put((generation,None,None,str(error)))
            else:
                self.jobs.put((generation,callback,result,""))
        Thread(target=worker,daemon=True).start()

    def poll(self):
        try:
            while True:
                generation,callback,result,error = self.jobs.get_nowait()
                if generation != self.generation:
                    continue
                if error:
                    self.status.set(error)
                    if callback is None and self.styles and self.style.get() in self.styles:
                        self.select.configure(state="normal")
                else:
                    callback(result)
        except Empty:
            pass
        if self.window.winfo_exists():
            self.window.after(50,self.poll)

    def load(self, _=None):
        self.generation += 1
        self.fonts,self.styles = {},{}
        for box in (self.family,self.style):
            box.set("")
            box.configure(values=())
        self.select.configure(state="disabled")
        self.sample.configure(image="",text="Bitmap sample",height=4)
        self.pick.grid_remove()
        self.family.grid()
        self.filter_box.configure(state="normal")
        self.search.set("")
        source = self.source.get()
        if source == FONT_FILE:
            self.family.grid_remove()
            self.filter_box.configure(state="disabled")
            self.pick.grid(row=1,column=1,sticky="ew")
            self.status.set("Choose a TrueType or OpenType font file.")
            return

        def catalog():
            from glyph_bridge.gui import google_catalog, local_catalog
            if source == GOOGLE:
                families,_ = google_catalog()
                return {family:family for family in families}
            return local_catalog()
        self.run(catalog,self.catalog_ready,"Loading typefaces…")

    def filter_families(self):
        query = self.search.get().strip().casefold()
        choices = sorted((family for family in self.fonts if query in family.casefold()),key=str.casefold)
        self.family.configure(values=choices)
        return choices

    def catalog_ready(self, result):
        self.fonts = result
        families = self.filter_families()
        if families:
            selected = "Inter" if "Inter" in families else families[0]
            self.family.set(selected)
            self.load_styles()
        else:
            self.status.set("No typefaces found. Try Installed fonts or Font file.")

    def font_file(self):
        file = filedialog.askopenfilename(parent=self.window,
                                          filetypes=[("Font files","*.ttf *.otf *.ttc")])
        if file:
            self.catalog_ready({Path(file).stem:Path(file)})

    def load_styles(self, _=None):
        family = self.family.get()
        value = self.fonts.get(family)
        if not value:
            return
        local = self.source.get() != GOOGLE
        self.styles = {}
        self.style.set("")
        self.style.configure(values=())

        def discover():
            from glyph_bridge.gui import GOOGLE_SOURCE, LOCAL_SOURCE, font_options
            return {selection.style:selection for selection in
                    font_options(LOCAL_SOURCE if local else GOOGLE_SOURCE,str(value)).values()}
        self.run(discover,self.styles_ready,"Loading available styles…")

    def styles_ready(self, result):
        self.styles = result
        styles = sorted(result,key=str.casefold)
        self.style.configure(values=styles)
        self.style.set("Regular" if "Regular" in result else styles[0])
        self.preview()

    def selection(self):
        try:
            return self.styles[self.style.get()], self.read_size()
        except (KeyError,ValueError):
            return None

    def preview(self, _=None):
        choice = self.selection()
        if not choice:
            return

        def bitmap():
            from glyph_bridge.gui import bitmap_preview
            image,_ = bitmap_preview(*choice,"Aa Bb 0123",None)
            return image

        def ready(image):
            scale = max(1,min(3,360//image.width,105//image.height))
            bitmap = Image.new("RGB",image.size,"#111923")
            bitmap.paste("#82e7cd",(0,0,*image.size),image)
            self.photo = ImageTk.PhotoImage(bitmap.resize((image.width*scale,image.height*scale),
                                                          Image.Resampling.NEAREST),
                                             master=self.window)
            self.sample.configure(image=self.photo,text="",height=0)
            self.status.set("Choose Use typeface to embed this bitmap font in the project.")
            self.select.configure(state="normal")
        self.run(bitmap,ready,"Drawing bitmap sample…")

    def accept(self):
        choice = self.selection()
        if not choice:
            return

        def ready(data):
            self.apply(data)
            self.window.destroy()
        self.run(lambda: rasterize(*choice),ready,"Rasterizing printable ASCII…")


@lru_cache(maxsize=8)
def glyph_cells(header):
    """Read bitmap cells emitted by Glyph Bridge for an on-canvas preview."""
    arrays = {}
    for name,height,width,body in re.findall(
            r"static const bool (\w+)\[(\d+)\]\[(\d+)\] PROGMEM = \{(.*?)\};",
            header,re.S):
        size = (int(width),int(height))
        pixels = bytes(255 if bit == "1" else 0 for bit in re.findall(r"\b[01]\b",body))
        if len(pixels) == size[0]*size[1]:
            arrays[name] = Image.frombytes("L",size,pixels)
    return {chr(int(code)):arrays[name] for code,name in re.findall(
        r"case (\d+): return \{ &(\w+)\[0\]\[0\]",header) if name in arrays and int(code) <= 127}
