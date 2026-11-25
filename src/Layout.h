#pragma once
#include "Panel.h"
#include "Widgets.h"

struct PanelFlexItem
{
    BaseComponent* widget;
    int basis;
    unsigned weight;
};

template <size_t WIDTH, size_t HEIGHT>
class PanelFlex : public Panel<WIDTH, HEIGHT, false>
{
    const PanelFlexItem* items = nullptr;
    size_t count = 0;
    PanelAxis axis = PanelAxis::VERTICAL;
    int padding = 0, gap = 0;

public:
    using Panel<WIDTH, HEIGHT, false>::Panel;
    void setLayout(const PanelFlexItem* entries, size_t length,
                   PanelAxis direction = PanelAxis::VERTICAL, int spacing = 0, int inset = 0)
    {
        items = entries;
        count = items ? length : 0;
        axis = direction;
        gap = max(0, spacing);
        padding = max(0, inset);
        relayout();
    }
    void setSize(int w, int h) override
    {
        const auto old = this->getSize();
        Panel<WIDTH, HEIGHT, false>::setSize(w, h);
        const auto size = this->getSize();
        if (size.dx != old.dx || size.dy != old.dy) relayout();
    }
    void relayout()
    {
        const auto size = this->getSize();
        const bool vertical = axis == PanelAxis::VERTICAL;
        const int main = vertical ? size.dy : size.dx;
        const int cross = vertical ? size.dx : size.dy;
        int64_t used = int64_t(padding) * 2;
        uint64_t weights = 0;
        bool haveItem = false;
        for (size_t i = 0; i < count; ++i)
        {
            if (!items[i].widget || !items[i].widget->isVisible()) continue;
            used += max(0, items[i].basis) + (haveItem ? int64_t(gap) : 0);
            weights += items[i].weight;
            haveItem = true;
        }
        const int extra = panelCoordinate(max(int64_t(0), int64_t(main) - used));
        const int crossSize = panelCoordinate(max(int64_t(0), int64_t(cross) - int64_t(padding) * 2));
        int cursor = padding;
        uint64_t progress = 0;
        for (size_t i = 0; i < count; ++i)
        {
            auto* widget = items[i].widget;
            if (!widget || !widget->isVisible()) continue;
            const auto before = progress;
            progress += items[i].weight;
            const int share = weights ? int(uint64_t(extra) * progress / weights -
                                            uint64_t(extra) * before / weights) : 0;
            const int length = panelCoordinate(int64_t(max(0, items[i].basis)) + share);
            widget->setLoc(vertical ? padding : cursor, vertical ? cursor : padding);
            widget->setSize(vertical ? crossSize : length, vertical ? length : crossSize);
            cursor = panelCoordinate(int64_t(cursor) + length + gap);
        }
    }
};

inline void panelGrid(BaseComponent* const* items, size_t count, unsigned columns,
                      PixmapRect area, int horizontalGap = 0, int verticalGap = 0)
{
    if (!items || !count || !columns || area.width < 0 || area.height < 0) return;
    const int64_t rows = int64_t((count - 1) / columns + 1);
    const int xGap = columns > 1 ? max(0, min(horizontalGap, int(int64_t(area.width) / (columns - 1)))) : 0;
    const int yGap = rows > 1 ? max(0, min(verticalGap, int(area.height / (rows - 1)))) : 0;
    const int64_t width = max(int64_t(0), int64_t(area.width) - int64_t(xGap) * (columns - 1));
    const int64_t height = max(int64_t(0), int64_t(area.height) - int64_t(yGap) * (rows - 1));
    for (size_t i = 0; i < count; ++i)
    {
        if (!items[i]) continue;
        const int64_t col = i % columns, row = i / columns;
        const auto left = width * col / columns, top = height * row / rows;
        items[i]->setLoc(panelCoordinate(int64_t(area.x) + left + col * xGap),
                         panelCoordinate(int64_t(area.y) + top + row * yGap));
        items[i]->setSize(int(width * (col + 1) / columns - left),
                          int(height * (row + 1) / rows - top));
    }
}
