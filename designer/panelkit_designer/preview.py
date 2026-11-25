"""Desktop layout preview. Actual bitmap rasterization happens on the device."""

import calendar
import math

from PIL import Image, ImageOps, ImageTk

from .project import values

BG = "#192635"
SURFACE = "#223345"
BORDER = "#3e5163"
ACCENT = "#3ea7a2"
INK = "#ffffff"
MUTED = "#9fb2c4"

def intersect(first, second):
    x, y = max(first[0], second[0]), max(first[1], second[1])
    right = min(first[0] + first[2], second[0] + second[2])
    bottom = min(first[1] + first[3], second[1] + second[3])
    return (x, y, max(0, right-x), max(0, bottom-y))


def layout(project):
    """Yield each widget with screen coordinates and its effective clipping box."""
    siblings = {}
    for widget in project.widgets:
        siblings.setdefault(widget.parent, []).append(widget)
    hidden = set()
    for widget in project.widgets:
        if widget.kind == "Tabs":
            data = values(widget)
            current = max(0, min(len(data["pages"]) - 1, data["selected"]))
            hidden.update(name for i, (_, name) in enumerate(data["pages"]) if i != current)

    def walk(parent, x, y, width, height, clip):
        name = parent.name if parent else ""
        members = siblings.get(name, [])
        slots = {}
        if parent and parent.kind == "Flex":
            p = values(parent)
            vertical = p["axis"] == "VERTICAL"
            entries = p["layout"] or [[child.name, 0, 1] for child in members]
            entries = [(key, base, weight) for key, base, weight in entries
                       if project.widget(key).visible and key not in hidden]
            inset, gap = p["padding"], p["gap"]
            extra = max(0, (height if vertical else width) - inset * 2
                        - max(0, len(entries)-1) * gap - sum(e[1] for e in entries))
            cross = max(0, (width if vertical else height) - inset * 2)
            total = sum(e[2] for e in entries)
            cursor, used = inset, 0
            for key, base, weight in entries:
                share = extra * (used + weight) // total - extra * used // total if total else 0
                used += weight
                size = base + share
                child = project.widget(key)
                cw, ch = (cross, size) if vertical else (size, cross)
                d = values(child)
                fixed = child.kind in ("Button", "Label", "List", "Pixmap", "Sprite") or (
                    child.kind in ("Panel", "Plot") and d["buffered"])
                if fixed:
                    cw, ch = min(cw, child.width), min(ch, child.height)
                slots[key] = (inset, cursor, cw, ch) if vertical else (cursor, inset, cw, ch)
                cursor += size + gap
        if parent and parent.kind == "Panel" and (cols := values(parent)["grid_columns"]) and members:
            rows = (len(members) + cols - 1) // cols
            gx, gy = values(parent)["grid_gap"]
            gx = max(0, min(gx, width // (cols - 1))) if cols > 1 else 0
            gy = max(0, min(gy, height // (rows - 1))) if rows > 1 else 0
            space_x, space_y = width - gx*(cols-1), height - gy*(rows-1)
            for i, child in enumerate(members):
                row, col = divmod(i, cols)
                left, top = space_x*col//cols, space_y*row//rows
                cw, ch = space_x*(col+1)//cols-left, space_y*(row+1)//rows-top
                slots[child.name] = (left+col*gx,top+row*gy,cw,ch)

        for widget in sorted(members, key=lambda child: child.order):
            if not widget.visible or widget.name in hidden:
                continue
            p = values(widget)
            box = slots.get(widget.name, (widget.x, widget.y, widget.width, widget.height))
            if parent and parent.kind == "Window" and len(members) == 1 and values(parent)["fit_single_child"]:
                box = (0, 0, width, height)
            wx, wy, ww, wh = box
            if widget.kind == "Window" and p["maximized"]:
                wx, wy, ww, wh = 0, 0, width, height
            if widget.kind == "Window" and p["minimized"] and not p["maximized"]:
                wh = min(24, wh)
            if ww <= 0 or wh <= 0:
                continue
            bounds = (x+wx, y+wy, ww, wh)
            clipped = intersect(clip, bounds)
            if not clipped[2] or not clipped[3]:
                continue
            yield widget, bounds, clipped
            if widget.kind == "Window":
                client = (x+wx, y+wy+24, ww, max(0, wh-24))
                yield from walk(widget, *client, intersect(clipped, client))
            elif widget.kind == "Scroll Area":
                sx = max(0, min(p["scroll"][0], p["content_size"][0] - ww))
                sy = max(0, min(p["scroll"][1], p["content_size"][1] - wh))
                yield from walk(widget, x+wx-sx, y+wy-sy, ww, wh, clipped)
            else:
                yield from walk(widget, x+wx, y+wy, ww, wh, clipped)

    screen = (0, 0, project.width, project.height)
    yield from walk(None, *screen, screen)


class Preview:
    def __init__(self, canvas):
        self.canvas = canvas
        self.images = {}
        self.drawn = []
        self.origin = (22, 22)
        self.scale = 1
        self.boxes = {}
        self.hit_boxes = {}

    def image(self, project, name, width, height, mode, region=None, pose=None, clip=None):
        source = project.asset(name)
        updated = source.stat().st_mtime_ns
        key = (source, updated, width, height, mode, clip, tuple(region or ()),
               tuple(sorted((pose or {}).items())))
        if key in self.images:
            return self.images[key]
        with Image.open(source) as opened:
            if opened.width * opened.height > 1048576:
                raise ValueError("Image exceeds the import limit")
            image = ImageOps.exif_transpose(opened).convert("RGBA")
        if region:
            x, y, w, h = region
            image = image.crop((x, y, x+w, y+h))
        if pose:
            sx, sy = pose.get("scaleX", 1), pose.get("scaleY", 1)
            w, h = (max(1, min(3*max(width,height), round(image.width*abs(sx)*self.scale))),
                    max(1, min(3*max(width,height), round(image.height*abs(sy)*self.scale))))
            image = image.resize((w, h), Image.Resampling.NEAREST)
            if sx < 0:
                image = ImageOps.mirror(image)
            if sy < 0:
                image = ImageOps.flip(image)
            image = image.rotate(-(pose.get("angle", 0) % 360), expand=True)
            layer = Image.new("RGBA", (width, height))
            layer.paste(image, (round(pose.get("x", width/2/self.scale)*self.scale-image.width/2),
                                round(pose.get("y", height/2/self.scale)*self.scale-image.height/2)))
            image = layer
        elif mode == "STRETCH":
            image = image.resize((width, height))
        elif mode == "FILL":
            image = ImageOps.fit(image, (width, height))
        else:
            if mode == "FIT":
                image = ImageOps.contain(image, (width, height))
            else:
                image = image.resize((max(1, round(image.width*self.scale)),
                                      max(1, round(image.height*self.scale))))
            layer = Image.new("RGBA", (width, height))
            x = (width-image.width)//2 if mode in ("FIT", "CENTER") else (
                width-image.width if mode.endswith("RIGHT") else 0)
            y = (height-image.height)//2 if mode in ("FIT", "CENTER") else (
                height-image.height if mode.startswith("BOTTOM") else 0)
            layer.paste(image, (x,y))
            image = layer
        photo = ImageTk.PhotoImage(image.crop(clip) if clip else image)
        if len(self.images) >= 32:
            self.images.clear()
        self.images[key] = photo
        return photo

    def draw(self, project, selection, scale):
        self.scale = scale
        c = self.canvas
        c.delete("all")
        self.drawn.clear()
        self.boxes.clear()
        self.hit_boxes.clear()
        ox, oy = self.origin
        right, bottom = ox+project.width*scale, oy+project.height*scale
        c.create_rectangle(ox, oy, right, bottom, fill=project.background, outline=BORDER)
        c.configure(scrollregion=(0,0,max(right+22,c.winfo_width()),max(bottom+22,c.winfo_height())))
        for widget, box, clip in layout(project):
            self.boxes[widget.name] = box
            self.hit_boxes[widget.name] = clip
            self.widget(project, widget, box, clip)
        if selection in self.boxes:
            x, y, w, h = self.boxes[selection]
            left, top = ox+x*scale, oy+y*scale
            r, b = left+w*scale, top+h*scale
            c.create_rectangle(left, top, r, b, outline="#82e7cd", width=2)
            c.create_rectangle(r-4, b-4, r+4, b+4, fill="#82e7cd", outline=BG, tags=("resize",))
        c.create_text(ox, bottom+12, anchor="w", fill=MUTED,
                      text=f"{project.width} × {project.height}  •  {scale:.0%}",
                      font=("Segoe UI", 9))

    def widget(self, project, widget, box, clip):
        x, y, w, h = box
        clip_x, clip_y, clip_w, clip_h = clip
        clip_left, clip_top = clip_x - x, clip_y - y
        clip_right, clip_bottom = clip_left+clip_w, clip_top+clip_h
        x, y = self.origin[0]+x*self.scale, self.origin[1]+y*self.scale
        s, c = self.scale, self.canvas
        tag = ("widget", widget.name)
        p, kind = values(widget), widget.kind
        theme = project.theme | p.get("theme", {})
        bg, surface = theme.get("background", BG), theme.get("surface", SURFACE)
        border, accent, ink = (theme.get(key, default) for key, default in
                               (("border", BORDER), ("accent", ACCENT), ("text", INK)))

        def rect(a, b, d, e, fill, outline=""):
            a,b,d,e = max(a,clip_left),max(b,clip_top),min(d,clip_right),min(e,clip_bottom)
            if a >= d or b >= e:
                return
            c.create_rectangle(x+a*s, y+b*s, x+d*s, y+e*s, fill=fill, outline=outline, tags=tag)

        def text(a, b, label, max_width=None, color=ink, anchor="w"):
            if not label or not (clip_left <= a < clip_right and clip_top <= b < clip_bottom):
                return
            space = clip_right-a if anchor == "w" else 2*min(a-clip_left,clip_right-a)
            limit = int(min(space,max_width if max_width is not None else w-a) / 7)
            if limit <= 0:
                return
            shown = label if len(label) <= limit else label[:max(0,limit-1)] + "…"
            c.create_text(x+a*s, y+b*s, text=shown, fill=color, anchor=anchor,
                          font=("Segoe UI", max(7, round(theme.get("text_height", 14)*s*0.66))),
                          tags=tag)

        rect(0,0,w,h,"")
        if kind in ("Panel", "Flex", "Scroll Area", "Window"):
            rect(0,0,w,h,p.get("background", bg),border)
            if kind == "Window":
                rect(0,0,w,min(h,24),surface)
                text(6,min(h,24)/2,p["title"],max(0,w-66))
                for n, mark in enumerate(("×", "□", "–")):
                    text(w-16-n*18,min(h,24)/2,mark,14,anchor="center")
            if kind == "Scroll Area":
                rect(w-4,2,w,h-2,border)
            return
        if kind in ("Picture", "Pixmap", "Sprite"):
            if p["image"]:
                try:
                    cropped = (round(clip_left*s),round(clip_top*s),
                               max(round(clip_left*s)+1,round(clip_right*s)),
                               max(round(clip_top*s)+1,round(clip_bottom*s)))
                    if w*s*h*s > 16000000:
                        raise ValueError("Preview is too large at this zoom")
                    photo = self.image(project, p["image"], max(1,round(w*s)), max(1,round(h*s)),
                                       p.get("mode", "FIT"), p.get("region"), p.get("pose"),cropped)
                    self.drawn.append(photo)
                    c.create_image(x+cropped[0],y+cropped[1],image=photo,anchor="nw",tags=tag)
                    return
                except (OSError, ValueError):
                    pass
            rect(0,0,w,h,surface,border)
            text(5,h/2,"Choose an image",color=MUTED)
            return
        if kind in ("Text", "Label"):
            text(4,h/2,p["text"],color=p.get("color", ink))
            return
        if kind in ("Slider", "Toggle", "Progress"):
            track, fill = p["colors"] or [surface, accent]
            lo, hi = p.get("range", [0,1])[:2]
            fraction = int(p["checked"]) if kind == "Toggle" else max(0,min(1,(p["value"]-lo)/(hi-lo)))
            rect(0,0,w,h,track,border)
            rect(0,0,w*fraction,h,fill)
            if kind != "Progress":
                left = max(0,min(w-8,w*fraction-4))
                rect(left,2,min(w,left+8),h-2,ink)
            return
        if kind == "Plot":
            ground, trace = p["colors"] or ["#142030", accent]
            rect(0,0,w,h,ground,border)
            for i in range(1,4):
                rect(1,h*i/4,w-1,h*i/4+0.7,border)
            samples = p["samples"] or ([math.sin(i*0.25) for i in range(32)] if p["demo_signal"] else [])
            if len(samples) > 1:
                lo, hi = p["range"]
                coords = []
                for index, value in enumerate(samples[-p["capacity"]:]):
                    trace_x = 1+index*(w-2)/max(1,len(samples[-p["capacity"]:])-1)
                    trace_y = 1+(h-2)*(1-max(0,min(1,(value-lo)/(hi-lo))))
                    coords.extend((x+max(clip_left,min(clip_right,trace_x))*s,
                                   y+max(clip_top,min(clip_bottom,trace_y))*s))
                c.create_line(*coords,fill=trace,width=max(1,s),tags=tag)
            return
        if kind == "Calendar":
            year, month, day = p["date"]
            rect(0,0,w,h,bg,border)
            rect(0,0,w,20,surface)
            text(4,10,f"<   {calendar.month_abbr[month]} {year}   >",w-8)
            for row, days in enumerate([["S","M","T","W","T","F","S"]] +
                                       calendar.Calendar(6).monthdayscalendar(year,month)):
                for col, number in enumerate(days):
                    if number:
                        cw,ch = w/7,(h-20)/7
                        a,b = col*cw,20+row*ch
                        rect(a,b,a+cw-1,b+ch-1,accent if number == day else surface)
                        text(a+1,b+ch/2,str(number),max(7,cw))
            return
        if kind == "Table":
            rect(0,0,w,h,bg,border)
            row_h, header_h = p["row_height"], min(h,p["header_height"])
            left = 0
            for col, (label, col_w) in enumerate(p["columns"]):
                end = min(w,left+col_w)
                rect(left,0,end,header_h,surface,border)
                text(left+3,header_h/2,label,end-left-4)
                for row, cells in enumerate(p["rows"]):
                    top = header_h+row*row_h
                    if top+row_h > h:
                        break
                    rect(left,top,end,top+row_h,bg,border)
                    text(left+3,top+row_h/2,cells[col],end-left-4)
                left += col_w
                if left >= w:
                    break
            return
        if kind in ("List", "Radio Group", "Tabs", "Keypad"):
            items = p.get("items", [entry[0] for entry in p.get("pages", [])])
            if kind == "Keypad":
                items = list("123456789-0‹")
            if not items:
                items = ["Add tab pages"] if kind == "Tabs" else []
            columns = len(items) if kind == "Tabs" else (3 if kind == "Keypad" else 1)
            rows = max(1, math.ceil(len(items)/max(1,columns)))
            row_height = p.get("row_height", h/rows)
            rect(0,0,w,h,bg,border)
            for i, label in enumerate(items):
                a,b = i%columns*w/columns,i//columns*row_height
                end = a+w/columns
                if b + row_height > h:
                    break
                rect(a,b,end,b+row_height,accent if i == p.get("selected",-1) else surface,border)
                text(a+3,b+row_height/2,label,end-a-5)
            return
        rect(0,0,w,h,surface,border)
        if kind == "Button":
            colors = p["colors"]
            if colors:
                rect(0,0,w,h,colors[0],colors[3] if len(colors)>3 else border)
            text(w/2,h/2,p["text"],w-6,colors[1] if colors else ink,anchor="center")
        elif kind == "Combo Box":
            items = p["items"]
            text(5,h/2,items[min(p["selected"],len(items)-1)] if items else "",w-20)
            text(w-15,h/2,"▾",14)
        elif kind == "Spin Box":
            text(5,h/2,"−",10)
            text(w/2,h/2,str(p["value"]),w-25,anchor="center")
            text(w-15,h/2,"+",14)
        elif kind == "Check Box":
            side = min(17,h-2)
            rect(4,(h-side)/2,4+side,(h+side)/2,accent if p["checked"] else bg,border)
            text(25,h/2,p["text"],w-27)
        else:
            text(4,h/2,p.get("text",kind))

