#pragma once
#include <stdio.h>
#include "Controls.h"

struct PanelDate
{
    int year, month, day;
};

class PanelCalendar : public PanelControl
{
    static const int firstYear = 1, lastYear = 9999;
    PanelDate selected{2026, 1, 1};
    int year = 2026, month = 1;
    void (*changed)(PanelDate) = nullptr;

    static int days(int y, int m)
    {
        const uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        return lengths[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    }
    static int weekday(int y, int m)
    {
        const int adjustment[] = {0,3,2,5,0,3,5,1,4,6,2,4};
        if (m < 3) --y;
        return (y + y / 4 - y / 100 + y / 400 + adjustment[m - 1] + 1) % 7;
    }
    int header() const { return min(22, dim.dy / 3); }
    void changeMonth(int delta)
    {
        const int32_t serial = int32_t(year - 1) * 12 + month - 1 + delta;
        if (serial < 0 || serial >= int32_t(lastYear) * 12) return;
        year = serial / 12 + 1;
        month = serial % 12 + 1;
        invalidate();
    }

public:
    using PanelControl::PanelControl;
    bool setDate(PanelDate date)
    {
        if (date.year < firstYear || date.year > lastYear || date.month < 1 ||
            date.month > 12 || date.day < 1 || date.day > days(date.year, date.month)) return false;
        selected = date;
        year = date.year;
        month = date.month;
        invalidate();
        return true;
    }
    PanelDate getDate() const { return selected; }
    void onChange(void (*callback)(PanelDate)) { changed = callback; }
    void handleEvent(Event event) override
    {
        if ((event.type != EventType::CLICK && event.type != EventType::DOUBLE_CLICK) ||
            !inBounds(event.endX, event.endY)) return;
        const int x = event.endX - ulhc.x, y = event.endY - ulhc.y;
        const int bar = header(), tableHeight = dim.dy - bar;
        if (y < bar)
        {
            if (x < dim.dx / 5) changeMonth(-1);
            else if (x >= dim.dx - dim.dx / 5) changeMonth(1);
            return;
        }
        if (!tableHeight) return;
        const int row = int(((int64_t(y - bar) + 1) * 7 - 1) / tableHeight);
        const int col = int(((int64_t(x) + 1) * 7 - 1) / dim.dx);
        if (row == 0) return;
        const int day = (row - 1) * 7 + col - weekday(year, month) + 1;
        if (day >= 1 && day <= days(year, month))
        {
            selected = {year, month, day};
            invalidate();
            if (changed) changed(selected);
        }
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        static const char* const months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        static const char* const labels[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
        const auto rect = panelTranslate(bounds(), x, y);
        const int bar = header(), tableHeight = rect.height - bar;
        const int button = rect.width / 5;
        canvas.fill(rect, theme.background);
        canvas.fill({rect.x,rect.y,rect.width,bar}, theme.surface);
        char label[18];
        snprintf(label, sizeof(label), "%s %d", months[month - 1], year);
        caption(canvas, {rect.x + button, rect.y, rect.width - 2 * button, bar}, label);
        caption(canvas, {rect.x, rect.y, button, bar}, "<");
        caption(canvas, {rect.x + rect.width - button, rect.y, button, bar}, ">");
        const int first = weekday(year, month);
        for (int row = 0; row < 7; ++row)
        {
            const int top = rect.y + bar + int(int64_t(row) * tableHeight / 7);
            const int bottom = rect.y + bar + int(int64_t(row + 1) * tableHeight / 7);
            for (int col = 0; col < 7; ++col)
            {
                const int left = rect.x + int(int64_t(col) * rect.width / 7);
                const int right = rect.x + int(int64_t(col + 1) * rect.width / 7);
                const PixmapRect box{left, top, max(0, right - left - 1), max(0, bottom - top - 1)};
                if (!row) { caption(canvas, box, labels[col]); continue; }
                const int day = (row - 1) * 7 + col - first + 1;
                if (day < 1 || day > days(year, month)) continue;
                const bool active = selected.year == year && selected.month == month && selected.day == day;
                canvas.fill(box, active ? theme.accent : theme.surface);
                snprintf(label, sizeof(label), "%d", day);
                caption(canvas, box, label);
            }
        }
        renderDirty = false;
    }
};

class PanelTabs : public PanelChoices
{
    BaseComponent* const* pages = nullptr;
    int pageCount = 0;

public:
    using PanelChoices::PanelChoices;
    void setPages(const char* const* labels, BaseComponent* const* widgets, int length)
    {
        pages = widgets;
        setItems(labels, widgets ? length : 0);
        pageCount = count;
        setCurrent(selected);
    }
    void setCurrent(int index)
    {
        if (index < 0 || index >= pageCount || index >= count || !pages) return;
        setSelected(index);
        for (int i = 0; i < pageCount; ++i)
            if (pages[i]) pages[i]->setVisible(i == index);
    }
    void handleEvent(Event event) override
    {
        if ((event.type != EventType::CLICK && event.type != EventType::DOUBLE_CLICK) ||
            !inBounds(event.endX, event.endY) || !count) return;
        const int index = int(((int64_t(event.endX - ulhc.x) + 1) * count - 1) / dim.dx);
        if (index == selected || index >= pageCount) return;
        setCurrent(index);
        if (changed) changed(index);
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        for (int index = 0; index < count; ++index)
        {
            const int left = rect.x + int(int64_t(index) * rect.width / count);
            const int right = rect.x + int((int64_t(index) + 1) * rect.width / count);
            const PixmapRect item{left, rect.y, right - left, rect.height};
            canvas.fill(item, index == selected ? theme.accent : theme.surface);
            caption(canvas, item, items[index]);
        }
        renderDirty = false;
    }
};

class PanelSpinBox : public PanelControl
{
    int low = 0, high = 100, value = 0, step = 1;
    void (*changed)(int) = nullptr;

public:
    using PanelControl::PanelControl;
    bool setRange(int minimum, int maximum, int increment = 1)
    {
        if (maximum < minimum || increment <= 0) return false;
        low = minimum; high = maximum; step = increment;
        setValue(value);
        return true;
    }
    void setValue(int next)
    {
        next = max(low, min(high, next));
        if (next == value) return;
        value = next;
        invalidate();
    }
    int getValue() const { return value; }
    void onChange(void (*callback)(int)) { changed = callback; }
    void handleEvent(Event event) override
    {
        if ((event.type != EventType::CLICK && event.type != EventType::DOUBLE_CLICK) ||
            !inBounds(event.endX, event.endY)) return;
        const int button = dim.dx / 4;
        const int x = event.endX - ulhc.x;
        const int old = value;
        if (x < button) setValue(panelCoordinate(int64_t(value) - step));
        else if (x >= dim.dx - button) setValue(panelCoordinate(int64_t(value) + step));
        if (old != value && changed) changed(value);
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        const int button = rect.width / 4;
        canvas.fill(rect, theme.background);
        canvas.fill({rect.x, rect.y, button, rect.height}, theme.accent);
        canvas.fill({rect.x + rect.width - button, rect.y, button, rect.height}, theme.accent);
        caption(canvas, {rect.x, rect.y, button, rect.height}, "-");
        caption(canvas, {rect.x + rect.width - button, rect.y, button, rect.height}, "+");
        char number[18];
        snprintf(number, sizeof(number), "%d", value);
        caption(canvas, {rect.x + button, rect.y, rect.width - 2 * button, rect.height}, number);
        renderDirty = false;
    }
};

class PanelTextField : public PanelControl
{
    char text[65]{};
    size_t length = 0;
    void (*changed)(const char*) = nullptr;

public:
    using PanelControl::PanelControl;
    void setText(const char* value)
    {
        strlcpy(text, value ? value : "", sizeof(text));
        length = strlen(text);
        invalidate();
    }
    const char* getText() const { return text; }
    void onChange(void (*callback)(const char*)) { changed = callback; }
    void handleEvent(Event event) override
    {
        if (event.type != EventType::TYPE) return;
        const char c = event.character;
        if (c == '\b' || c == '\x7f')
        {
            if (!length) return;
            text[--length] = '\0';
        }
        else if (c >= 32 && c <= 126 && length < sizeof(text) - 1)
        {
            text[length++] = c;
            text[length] = '\0';
        }
        else return;
        invalidate();
        if (changed) changed(text);
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        canvas.fill(rect, theme.surface);
        const int underline = min(2, rect.height);
        canvas.fill({rect.x, rect.y + rect.height - underline, rect.width, underline}, theme.accent);
        const char* visible = text;
        if (theme.font && theme.font->getHeight() > 0)
        {
            int width = panelTextWidth(text, *theme.font, theme.textHeight);
            const int available = rect.width - 2 * min(theme.padding, rect.width / 2);
            while (*visible && width > available)
            {
                width -= panelGlyphWidth(theme.font->lookupChar(*visible), *visible,
                                         theme.textHeight, theme.font->getHeight());
                ++visible;
            }
        }
        caption(canvas, rect, visible);
        renderDirty = false;
    }
};

class PanelKeypad : public PanelControl
{
    void (*entered)(char) = nullptr;

    static char key(int index)
    {
        const char values[] = {'1','2','3','4','5','6','7','8','9','-','0','\b'};
        return values[index];
    }

public:
    using PanelControl::PanelControl;
    void onKey(void (*callback)(char)) { entered = callback; }
    void handleEvent(Event event) override
    {
        if (!entered || (event.type != EventType::CLICK && event.type != EventType::DOUBLE_CLICK) ||
            !inBounds(event.endX, event.endY)) return;
        const int col = int(((int64_t(event.endX - ulhc.x) + 1) * 3 - 1) / dim.dx);
        const int row = int(((int64_t(event.endY - ulhc.y) + 1) * 4 - 1) / dim.dy);
        entered(key(row * 3 + col));
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 3; ++col)
            {
                const int left = int(int64_t(col) * rect.width / 3);
                const int right = int(int64_t(col + 1) * rect.width / 3);
                const int top = int(int64_t(row) * rect.height / 4);
                const int bottom = int(int64_t(row + 1) * rect.height / 4);
                const PixmapRect cell{rect.x + left, rect.y + top,
                    max(0, right - left - 1), max(0, bottom - top - 1)};
                canvas.fill(cell, theme.surface);
                char label[] = {key(row * 3 + col), '\0'};
                caption(canvas, cell, label[0] == '\b' ? "<" : label);
            }
        renderDirty = false;
    }
};
