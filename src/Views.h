#pragma once
#include "Controls.h"

class PanelTable : public PanelControl
{
public:
    using CellText = const char* (*)(void*, int, int);
    using CellAction = void (*)(int, int);
    struct Column { const char* title; int width; };

private:
    const Column* columns = nullptr;
    CellText cellText = nullptr;
    CellAction action = nullptr;
    void* context = nullptr;
    int columnCount = 0, rowCount = 0;
    int rowHeight = 22, headerHeight = 22;
    int scrollX = 0, scrollY = 0, selectedRow = -1, selectedColumn = -1;

    int columnWidth(int i) const { return max(1, columns[i].width); }

public:
    using PanelControl::PanelControl;

    void setModel(const Column* fields, int fieldCount, int rows, CellText reader, void* data = nullptr)
    {
        columns = fields;
        columnCount = fields ? max(0, fieldCount) : 0;
        cellText = reader;
        context = data;
        rowCount = max(0, rows);
        scrollX = scrollY = 0;
        selectedRow = selectedColumn = -1;
        invalidate();
    }
    void setRowHeight(int value) { rowHeight = max(1, value); setScroll(scrollX, scrollY); invalidate(); }
    void setHeaderHeight(int value) { headerHeight = max(0, value); setScroll(scrollX, scrollY); invalidate(); }
    int getSelectedRow() const { return selectedRow; }
    int getSelectedColumn() const { return selectedColumn; }
    void onCellClick(CellAction callback) { action = callback; }

    void setScroll(int x, int y)
    {
        int64_t total = 0;
        for (int i = 0; i < columnCount; ++i) total += columnWidth(i);
        const int maxX = panelCoordinate(max(int64_t(0), total - dim.dx));
        const int maxY = panelCoordinate(max(int64_t(0), int64_t(rowCount) * rowHeight -
                                             max(0, dim.dy - headerHeight)));
        x = max(0, min(x, maxX));
        y = max(0, min(y, maxY));
        if (x == scrollX && y == scrollY) return;
        scrollX = x;
        scrollY = y;
        invalidate();
    }
    void setSize(int w, int h) override
    {
        PanelControl::setSize(w, h);
        setScroll(scrollX, scrollY);
    }
    void handleEvent(Event e) override
    {
        if (e.type == EventType::DRAG)
            setScroll(panelCoordinate(int64_t(scrollX) + e.startX - e.endX),
                      panelCoordinate(int64_t(scrollY) + e.startY - e.endY));
        else if (e.type == EventType::SCROLL && isfinite(e.direction) && e.direction)
            setScroll(scrollX, panelCoordinate(int64_t(scrollY) + (e.direction > 0 ? rowHeight : -rowHeight)));
        else if ((e.type == EventType::CLICK || e.type == EventType::DOUBLE_CLICK) &&
                 inBounds(e.endX, e.endY))
        {
            const int localY = e.endY - ulhc.y;
            const int row = localY < headerHeight ? -1 :
                panelCoordinate((int64_t(localY) - headerHeight + scrollY) / rowHeight);
            int64_t localX = int64_t(e.endX) - ulhc.x + scrollX;
            for (int col = 0; col < columnCount; ++col)
            {
                if (localX < columnWidth(col) && row < rowCount)
                {
                    selectedRow = row;
                    selectedColumn = col;
                    invalidate();
                    if (action) action(row, col);
                    break;
                }
                localX -= columnWidth(col);
            }
        }
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const auto rect = panelTranslate(bounds(), x, y);
        auto viewport = canvas;
        viewport.clip = panelIntersection(canvas.clip, rect);
        viewport.fill(rect, theme.background);
        auto body = viewport;
        body.clip = panelIntersection(viewport.clip,
            {rect.x, rect.y + headerHeight, rect.width, max(0, rect.height - headerHeight)});
        const int rowStart = scrollY / rowHeight;
        int64_t colX = int64_t(rect.x) - scrollX;
        for (int col = 0; col < columnCount && colX < int64_t(rect.x) + rect.width; ++col)
        {
            const int w = columnWidth(col);
            if (colX + w > rect.x)
            {
                int64_t rowY = int64_t(rect.y) + headerHeight - scrollY % rowHeight;
                for (int row = rowStart; row < rowCount && rowY < int64_t(rect.y) + rect.height; ++row)
                {
                    const PixmapRect cell{int(colX), int(rowY), w, rowHeight};
                    body.fill(cell, row == selectedRow && col == selectedColumn ? theme.accent :
                                     row % 2 ? theme.surface : theme.background);
                    if (cellText) caption(body, cell, cellText(context, row, col));
                    body.fill({cell.x, cell.y + rowHeight - 1, w, 1}, theme.border);
                    rowY += rowHeight;
                }
                const PixmapRect header{int(colX), rect.y, w, headerHeight};
                viewport.fill(header, theme.border);
                caption(viewport, header, columns[col].title);
            }
            colX += w;
        }
        renderDirty = false;
    }
};

template <size_t WIDTH, size_t HEIGHT>
class PanelScrollArea : public Panel<WIDTH, HEIGHT, false>
{
    int contentWidth = WIDTH, contentHeight = HEIGHT;
    int scrollX = 0, scrollY = 0;
    bool panning = false;
    int maxX() const { return max(0, contentWidth - this->dim.dx); }
    int maxY() const { return max(0, contentHeight - this->dim.dy); }

protected:
    Coord contentOffset() const override { return {-scrollX, -scrollY}; }

public:
    using Panel<WIDTH, HEIGHT, false>::Panel;
    void setContentSize(int w, int h)
    {
        contentWidth = max(0, w);
        contentHeight = max(0, h);
        setScroll(scrollX, scrollY);
        this->invalidate();
    }
    Coord getScroll() const { return {scrollX, scrollY}; }
    void setScroll(int x, int y)
    {
        x = max(0, min(x, maxX()));
        y = max(0, min(y, maxY()));
        if (x == scrollX && y == scrollY) return;
        scrollX = x;
        scrollY = y;
        this->invalidate();
    }
    void setSize(int w, int h) override
    {
        Panel<WIDTH, HEIGHT, false>::setSize(w, h);
        setScroll(scrollX, scrollY);
    }
    void cancelInteraction() override
    {
        panning = false;
        Panel<WIDTH, HEIGHT, false>::cancelInteraction();
    }
    void handleEvent(Event e) override
    {
        if (!this->isInteractive()) return;
        const auto loc = this->getLoc();
        if (e.type == EventType::SCROLL && this->inBounds(e.startX, e.startY) &&
            isfinite(e.direction) && e.direction)
        {
            setScroll(scrollX, panelCoordinate(int64_t(scrollY) + (e.direction > 0 ? 20 : -20)));
            return;
        }
        if (e.type == EventType::PRESS)
        {
            panning = this->inBounds(e.startX, e.startY) &&
                !this->hit(panelCoordinate(int64_t(e.startX) - loc.x + scrollX),
                           panelCoordinate(int64_t(e.startY) - loc.y + scrollY));
            if (panning) { this->cancelInput(); return; }
        }
        else if (panning && e.type == EventType::DRAG)
        {
            setScroll(panelCoordinate(int64_t(scrollX) + e.startX - e.endX),
                      panelCoordinate(int64_t(scrollY) + e.startY - e.endY));
            return;
        }
        else if (panning && e.type == EventType::RELEASE)
        {
            panning = false;
            return;
        }
        Panel<WIDTH, HEIGHT, false>::handleEvent(e);
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        Panel<WIDTH, HEIGHT, false>::render(canvas, x, y);
        const auto rect = panelTranslate(this->bounds(), x, y);
        if (maxY() && rect.width >= 3 && rect.height >= 6)
        {
            const int thumb = max(6, int(int64_t(rect.height) * rect.height / contentHeight));
            const int top = int(int64_t(scrollY) * (rect.height - thumb) / maxY());
            canvas.fill({rect.x + rect.width - 3, rect.y + top, 3, thumb}, RGB565(105, 135, 146));
        }
        if (maxX() && rect.height >= 3 && rect.width >= 6)
        {
            const int thumb = max(6, int(int64_t(rect.width) * rect.width / contentWidth));
            const int left = int(int64_t(scrollX) * (rect.width - thumb) / maxX());
            canvas.fill({rect.x + left, rect.y + rect.height - 3, thumb, 3}, RGB565(105, 135, 146));
        }
    }
};
