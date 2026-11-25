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

protected:
    void reflowChildren() override
    {
        if (items) relayout();
        else Panel<WIDTH, HEIGHT, false>::reflowChildren();
    }

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
        reflowChildren();
    }
    void setHorizontalLayout(const PanelFlexItem* entries, size_t length, int spacing = 0, int inset = 0)
    {
        setLayout(entries, length, PanelAxis::HORIZONTAL, spacing, inset);
    }
    void setVerticalLayout(const PanelFlexItem* entries, size_t length, int spacing = 0, int inset = 0)
    {
        setLayout(entries, length, PanelAxis::VERTICAL, spacing, inset);
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
            if (!this->contains(items[i].widget) || !items[i].widget->isVisible()) continue;
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
            if (!this->contains(widget) || !widget->isVisible()) continue;
            const auto before = progress;
            progress += items[i].weight;
            const int share = weights ? int(uint64_t(extra) * progress / weights -
                                            uint64_t(extra) * before / weights) : 0;
            const int length = panelCoordinate(int64_t(max(0, items[i].basis)) + share);
            widget->setLayoutGeometry({vertical ? padding : cursor, vertical ? cursor : padding,
                                       vertical ? crossSize : length, vertical ? length : crossSize});
            cursor = panelCoordinate(int64_t(cursor) + length + gap);
        }
    }
};
