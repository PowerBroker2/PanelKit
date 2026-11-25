#pragma once
#include "Component.h"

template <size_t WIDTH, size_t HEIGHT, bool BUFFERED = true>
class Panel : public Component<BUFFERED ? WIDTH : 1, BUFFERED ? HEIGHT : 1>, public ComponentRegistry
{
    using ComponentType = Component<BUFFERED ? WIDTH : 1, BUFFERED ? HEIGHT : 1>;
    static_assert(WIDTH > 0 && WIDTH <= INT_MAX && HEIGHT > 0 && HEIGHT <= INT_MAX,
                  "Panel dimensions must fit in int");
    MethodSlot<Panel, Event> eventSlot;
    uint16_t background = BLACK;
    bool subscribed = false;
    bool fullDamage = true;
    PixmapRect damage{};

    void receiveGlobal(Event e)
    {
        if (!this->registry)
            handleEvent(e);
    }

protected:
    virtual PixmapRect contentRect() const { return {0, 0, this->dim.dx, this->dim.dy}; }
    virtual Coord contentOffset() const { return {0, 0}; }

public:
    explicit Panel(ComponentRegistry* parentRegistry = nullptr)
        : ComponentType(parentRegistry), eventSlot(this, &Panel::receiveGlobal)
    {
        this->dim = {int(WIDTH), int(HEIGHT)};
        registryChanged();
    }

    ~Panel() override { if (subscribed) getEventSignal().detach(eventSlot); }
    void registryChanged() override
    {
        if (!this->registry && !subscribed)
        {
            getEventSignal().attach(eventSlot);
            subscribed = true;
        }
        else if (this->registry && subscribed)
        {
            getEventSignal().detach(eventSlot);
            subscribed = false;
        }
    }
    BaseComponent* ownerComponent() override { return this; }
    PixmapRect visibleArea() const override
    {
        const auto area = contentRect();
        const auto shift = contentOffset();
        return {-shift.x, -shift.y, area.width, area.height};
    }
    void invalidate() override
    {
        fullDamage = true;
        BaseComponent::invalidate();
    }
    void invalidateRegistry() override { this->invalidate(); }
    void invalidateRegion(PixmapRect region) override
    {
        const auto viewport = contentRect();
        const auto offset = contentOffset();
        region = panelTranslate(region, viewport.x + offset.x, viewport.y + offset.y);
        region = panelIntersection(region, viewport);
        if (!region.width || !region.height) return;
        this->renderDirty = true;
        const auto loc = this->getLoc();
        region = panelTranslate(region, loc.x, loc.y);
        if (this->registry) this->registry->invalidateRegion(region);
        else damage = panelUnion(damage, region);
    }
    void setSize(int width, int height) override
    {
        if (BUFFERED) ComponentType::setSize(width, height);
        else BaseComponent::setSize(width, height);
    }
    void deactivate() override { cancelInput(); BaseComponent::deactivate(); }
    void cancelInteraction() override { cancelInput(); }

    Panel& setBackground(uint16_t color)
    {
        if (background != color) { background = color; this->invalidate(); }
        return *this;
    }

    void handleEvent(Event e) override
    {
        if (!this->isInteractive()) return;
        const bool captured = e.type == EventType::DRAG || e.type == EventType::RELEASE;
        if ((e.type == EventType::CLICK || e.type == EventType::DOUBLE_CLICK) &&
            !this->inBounds(e.endX, e.endY)) return;
        if (!captured && e.type != EventType::TYPE && !this->inBounds(e.startX, e.startY))
        {
            if (e.type == EventType::PRESS) cancelInput();
            return;
        }
        const auto loc = this->getLoc();
        e.startX = panelCoordinate(int64_t(e.startX) - loc.x);
        e.endX = panelCoordinate(int64_t(e.endX) - loc.x);
        e.startY = panelCoordinate(int64_t(e.startY) - loc.y);
        e.endY = panelCoordinate(int64_t(e.endY) - loc.y);
        const auto area = contentRect();
        const bool clickEvent = e.type == EventType::CLICK || e.type == EventType::DOUBLE_CLICK;
        if (captured || e.type == EventType::TYPE ||
            (panelContains(area, e.startX, e.startY) && (!clickEvent || panelContains(area, e.endX, e.endY))))
        {
            const auto shift = contentOffset();
            auto childEvent = e;
            childEvent.startX = panelCoordinate(int64_t(e.startX) - area.x - shift.x);
            childEvent.endX = panelCoordinate(int64_t(e.endX) - area.x - shift.x);
            childEvent.startY = panelCoordinate(int64_t(e.startY) - area.y - shift.y);
            childEvent.endY = panelCoordinate(int64_t(e.endY) - area.y - shift.y);
            dispatch(childEvent);
        }
        else if (e.type == EventType::PRESS)
            cancelInput();
        ComponentType::handleEvent(e);
    }

    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const PixmapRect bounds = panelTranslate(this->bounds(), x, y);
        auto viewport = canvas;
        viewport.clip = panelIntersection(viewport.clip, bounds);
        if (viewport.clip.width && viewport.clip.height)
        {
            viewport.fill(bounds, background);
            if (BUFFERED) ComponentType::render(viewport, x, y);
            const auto client = panelTranslate(contentRect(), bounds.x, bounds.y);
            viewport.clip = panelIntersection(viewport.clip, client);
            const auto shift = contentOffset();
            renderChildren(viewport, client.x + shift.x, client.y + shift.y);
        }
        this->renderDirty = false;
    }

    // A root owns the display scene. Force a redraw after external drawing.
    template <typename Display>
    bool present(Display& display, bool force = false)
    {
        if (!force && !this->renderDirty) return false;
        auto canvas = PanelCanvas::from(display);
        if (!force && !fullDamage)
            canvas.clip = panelIntersection(canvas.clip, damage);
        fullDamage = false;
        damage = {};
        this->renderDirty = false;
        if (!canvas.clip.width || !canvas.clip.height) return false;
        canvas.fill(canvas.clip, background);
        if (this->isVisible()) render(canvas, 0, 0);
        display.swap();
        return true;
    }
};
