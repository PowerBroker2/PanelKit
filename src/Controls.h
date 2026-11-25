#pragma once
#include "Panel.h"
#include "Widgets.h"

struct PanelTheme
{
    uint16_t background = RGB565(25, 38, 53);
    uint16_t surface = RGB565(34, 51, 69);
    uint16_t border = RGB565(62, 81, 99);
    uint16_t accent = RGB565(62, 167, 162);
    uint16_t text = WHITE;
    const BitmapFont* font = nullptr;
    int textHeight = 14;
    int padding = 5;
    BitmapFilter fontFilter = BitmapFilter::PRESERVE_STROKES;
};

// A direct-drawn widget needs no separate framebuffer and can resize at runtime.
class PanelControl : public BaseComponent
{
protected:
    PanelTheme theme;
    void caption(const PanelCanvas& canvas, PixmapRect area, const char* text) const
    {
        const int padding = min(theme.padding, area.width / 2);
        area.x += padding;
        area.width -= padding * 2;
        if (area.height < theme.textHeight) return;
        area.y += (area.height - theme.textHeight) / 2;
        area.height = theme.textHeight;
        canvas.text(area, text, theme.font, theme.text, theme.textHeight, theme.fontFilter);
    }

public:
    PanelControl(int width, int height, ComponentRegistry* parent = nullptr) : BaseComponent(parent)
    {
        dim = {max(0, width), max(0, height)};
    }
    void setTheme(const PanelTheme& value)
    {
        theme = value;
        theme.textHeight = max(1, theme.textHeight);
        theme.padding = max(0, theme.padding);
        invalidate();
    }
};

class PanelText : public PanelControl
{
    char value[96]{};
    bool fit = false;

    void updateSize()
    {
        if (fit && theme.font)
            setSize(panelCoordinate(int64_t(panelTextWidth(value, *theme.font, theme.textHeight)) +
                                    int64_t(theme.padding) * 2),
                    panelCoordinate(int64_t(theme.textHeight) + int64_t(theme.padding) * 2));
    }

public:
    using PanelControl::PanelControl;
    bool isInteractive() const override { return false; }
    void setText(const char* text)
    {
        strlcpy(value, text ? text : "", sizeof(value));
        updateSize();
        invalidate();
    }
    void setTheme(const PanelTheme& value) { PanelControl::setTheme(value); updateSize(); }
    void setAutoSize(bool enabled) { fit = enabled; updateSize(); }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        caption(canvas, panelTranslate(bounds(), x, y), value);
        renderDirty = false;
    }
};

class PanelCheckBox : public PanelControl
{
    char label[48]{};
    bool checked = false;
    void (*changed)(bool) = nullptr;

public:
    using PanelControl::PanelControl;
    void setText(const char* value) { strlcpy(label, value ? value : "", sizeof(label)); invalidate(); }
    void setChecked(bool value) { if (value != checked) { checked = value; invalidate(); } }
    bool isChecked() const { return checked; }
    void onChange(void (*callback)(bool)) { changed = callback; }
    void handleEvent(Event event) override
    {
        if ((event.type == EventType::CLICK || event.type == EventType::DOUBLE_CLICK) &&
            inBounds(event.endX, event.endY))
        {
            setChecked(!checked);
            if (changed) changed(checked);
        }
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        const int side = min(16, min(rect.width, rect.height));
        if (!side) return;
        const int top = rect.y + (rect.height - side) / 2;
        canvas.fill({rect.x, top, side, side}, theme.border);
        if (side > 2)
            canvas.fill({rect.x + 1, top + 1, side - 2, side - 2},
                        checked ? theme.accent : theme.background);
        if (checked && side >= 8)
        {
            for (int i = 0; i < side / 4; ++i)
                canvas.fill({rect.x + 2 + i, top + side / 2 + i, 2, 2}, theme.text);
            for (int i = 0; i < side / 2; ++i)
                canvas.fill({rect.x + 2 + side / 4 + i, top + side * 3 / 4 - i, 2, 2}, theme.text);
        }
        caption(canvas, {rect.x + side, rect.y, rect.width - side, rect.height}, label);
        renderDirty = false;
    }
};

class PanelChoices : public PanelControl
{
protected:
    const char* const* items = nullptr;
    int count = 0;
    int selected = -1;
    void (*changed)(int) = nullptr;

    void choose(int index)
    {
        if (index < 0 || index >= count || index == selected) return;
        setSelected(index);
        if (changed) changed(selected);
    }

public:
    using PanelControl::PanelControl;
    virtual void setItems(const char* const* values, int length)
    {
        items = values;
        count = items ? max(0, length) : 0;
        selected = count ? 0 : -1;
        invalidate();
    }
    void setSelected(int index)
    {
        if (index < -1 || index >= count || index == selected) return;
        selected = index;
        invalidate();
    }
    int selectedIndex() const { return selected; }
    void onChange(void (*callback)(int)) { changed = callback; }
};

class PanelRadioGroup : public PanelChoices
{
public:
    using PanelChoices::PanelChoices;
    void handleEvent(Event event) override
    {
        if ((event.type == EventType::CLICK || event.type == EventType::DOUBLE_CLICK) &&
            count && inBounds(event.endX, event.endY))
            choose(int(((int64_t(event.endY - ulhc.y) + 1) * count - 1) / dim.dy));
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        if (!count || dim.dy <= 0 || dim.dx <= 0) return;
        const auto rect = panelTranslate(bounds(), x, y);
        for (int i = 0; i < count; ++i)
        {
            const int top = rect.y + int(int64_t(i) * dim.dy / count);
            const int bottom = rect.y + int((int64_t(i) + 1) * dim.dy / count);
            if (bottom <= top) continue;
            const int radius = min(7, (min(dim.dx, bottom - top) - 1) / 2);
            for (int row = -radius; radius > 0 && row <= radius; ++row)
            {
                const int half = int(sqrtf(float(radius * radius - row * row)));
                canvas.fill({rect.x + radius - half, (top + bottom) / 2 + row, 2 * half + 1, 1}, theme.border);
                const int r = radius - 2;
                if (r > 0 && abs(row) <= r)
                {
                    const int inner = int(sqrtf(float(r * r - row * row)));
                    canvas.fill({rect.x + radius - inner, (top + bottom) / 2 + row,
                                 2 * inner + 1, 1}, i == selected ? theme.accent : theme.background);
                }
            }
            const int indent = min(dim.dx, 2 * radius + 1);
            caption(canvas, {rect.x + indent, top, dim.dx - indent, bottom - top}, items[i]);
        }
        renderDirty = false;
    }
};

class PanelComboBox : public PanelChoices
{
    bool opened = false, above = false;
    int first = 0, maxRows = 5, visibleRows = 0;

    PixmapRect popupBounds() const
    {
        const int height = panelCoordinate(int64_t(dim.dy) * visibleRows);
        const int y = panelCoordinate(int64_t(ulhc.y) + (above ? -int64_t(height) : dim.dy));
        return {ulhc.x, y, dim.dx, height};
    }

public:
    using PanelChoices::PanelChoices;
    void setPopupRows(int value) { dismissPopup(); maxRows = max(1, min(32, value)); }
    void setSize(int width, int height) override
    {
        if (width != dim.dx || height != dim.dy) dismissPopup();
        PanelChoices::setSize(width, height);
    }
    void setItems(const char* const* values, int length) override
    {
        dismissPopup();
        PanelChoices::setItems(values, length);
    }
    PixmapRect visualBounds() const override
    {
        return opened ? panelUnion(bounds(), popupBounds()) : bounds();
    }
    bool hitTest(int x, int y) const override { return panelContains(visualBounds(), x, y); }
    void cancelInteraction() override {} // Capture and popup visibility are independent.
    void dismissPopup() override
    {
        if (!opened) return;
        invalidate();
        opened = false;
        if (registry) registry->setPopup(nullptr);
        invalidate();
    }
    void deactivate() override { dismissPopup(); BaseComponent::deactivate(); }
    void registryChanged() override { if (!registry) dismissPopup(); }
    void setVisible(bool value) override
    {
        if (!value) dismissPopup();
        BaseComponent::setVisible(value);
    }
    void handleEvent(Event event) override
    {
        if (!count || dim.dy <= 0) return;
        if (opened && (event.type == EventType::SCROLL || event.type == EventType::DRAG))
        {
            const float delta = event.type == EventType::SCROLL ? event.direction :
                                float(event.startY) - event.endY;
            if (isfinite(delta) && delta)
            {
                first = max(0, min(count - visibleRows, first + (delta > 0 ? 1 : -1)));
                invalidate();
            }
            return;
        }
        if ((event.type != EventType::CLICK && event.type != EventType::DOUBLE_CLICK) ||
            !hitTest(event.endX, event.endY)) return;
        if (opened)
        {
            const auto options = popupBounds();
            const int index = panelContains(options, event.endX, event.endY)
                ? first + (event.endY - options.y) / dim.dy : -1;
            dismissPopup();
            choose(index);
        }
        else if (registry)
        {
            const auto viewport = registry->visibleArea();
            const int below = panelCoordinate((int64_t(viewport.y) + viewport.height - ulhc.y - dim.dy) / dim.dy);
            const int upper = panelCoordinate((int64_t(ulhc.y) - viewport.y) / dim.dy);
            const int wanted = min(count, maxRows);
            above = below < wanted && upper > below;
            visibleRows = min(wanted, above ? upper : below);
            if (visibleRows <= 0) return;
            first = max(0, min(count - visibleRows, selected));
            opened = true;
            registry->setPopup(this);
            invalidate();
        }
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        auto rect = panelTranslate(bounds(), x, y);
        canvas.fill(rect, theme.surface);
        caption(canvas, {rect.x, rect.y, max(0, rect.width - 16), rect.height},
                selected >= 0 ? items[selected] : "");
        for (int i = 0; i < 4 && rect.width >= 14 && rect.height >= 8; ++i)
            canvas.fill({rect.x + rect.width - 12 + i, rect.y + rect.height / 2 - 1 + i,
                         8 - 2 * i, 1}, theme.text);
        if (opened)
        {
            auto row = panelTranslate(popupBounds(), x, y);
            row.height = dim.dy;
            for (int i = 0; i < visibleRows; ++i)
            {
                canvas.fill(row, (first + i == selected) ? theme.accent : theme.background);
                caption(canvas, row, items[first + i]);
                row.y += row.height;
            }
        }
        renderDirty = false;
    }
};
