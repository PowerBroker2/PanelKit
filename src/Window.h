#pragma once
#include "Controls.h"

template <size_t WIDTH, size_t HEIGHT>
class PanelWindow : public Panel<WIDTH, HEIGHT, false>
{
    enum class Gesture { NONE, MOVE, RESIZE };
    Gesture gesture = Gesture::NONE;
    PanelTheme theme;
    char title[64]{};
    enum class Snap { NONE, FULL, LEFT, RIGHT, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
    Snap snap = Snap::NONE;
    PixmapRect restore{}, moveOrigin{};
    bool minimized = false, dragged = false;
    bool moveEnabled = true, resizeEnabled = true, closeEnabled = true;
    bool fitClient = false;
    bool raiseOnPress = true;
    int openHeight = HEIGHT;
    int titleHeight = 24;
    int minWidth = 80, minHeight = 48;
    int snapDistance = 12;
    void (*closed)() = nullptr;

    int buttonWidth() const { return min(22, this->dim.dx / 4); }

    void layoutClient()
    {
        if (!fitClient || this->numComps() != 1) return;
        auto* client = this->headComponent();
        const auto area = contentRect();
        client->setLayoutGeometry({0, 0, area.width, area.height});
    }

    void setGeometry(PixmapRect rect)
    {
        this->setLoc(rect.x, rect.y);
        this->setSize(rect.width, rect.height);
    }

    Snap snapTarget(int x, int y) const
    {
        if (!snapDistance || !this->registry || !this->registry->ownerComponent() || minimized)
            return Snap::NONE;
        const auto area = this->registry->visibleArea();
        if (area.width <= 0 || area.height <= 0) return Snap::NONE;
        const int64_t right = int64_t(area.x) + area.width, bottom = int64_t(area.y) + area.height;
        const int xBand = min(snapDistance, area.width / 2);
        const int yBand = min(snapDistance, max(1, area.height / 2));
        const bool left = x < int64_t(area.x) + xBand;
        const bool rightSide = x >= right - xBand;
        const bool top = y < int64_t(area.y) + yBand;
        const bool lower = y >= bottom - yBand;
        if (left) return top ? Snap::TOP_LEFT : lower ? Snap::BOTTOM_LEFT : Snap::LEFT;
        if (rightSide) return top ? Snap::TOP_RIGHT : lower ? Snap::BOTTOM_RIGHT : Snap::RIGHT;
        return top ? Snap::FULL : Snap::NONE;
    }

    PixmapRect snappedGeometry(PixmapRect area) const
    {
        const int halfWidth = area.width / 2, halfHeight = area.height / 2;
        switch (snap)
        {
            case Snap::TOP_LEFT: return {area.x, area.y, halfWidth, halfHeight};
            case Snap::BOTTOM_LEFT: return {area.x, area.y + halfHeight, halfWidth, area.height - halfHeight};
            case Snap::LEFT: return {area.x, area.y, halfWidth, area.height};
            case Snap::TOP_RIGHT: return {area.x + halfWidth, area.y, area.width - halfWidth, halfHeight};
            case Snap::BOTTOM_RIGHT: return {area.x + halfWidth, area.y + halfHeight, area.width - halfWidth, area.height - halfHeight};
            case Snap::RIGHT: return {area.x + halfWidth, area.y, area.width - halfWidth, area.height};
            default: return area;
        }
    }

protected:
    void reflowChildren() override
    {
        if (fitClient && this->numComps() == 1) layoutClient();
        else Panel<WIDTH, HEIGHT, false>::reflowChildren();
    }
    PixmapRect contentRect() const override
    {
        return {0, min(titleHeight, this->dim.dy), this->dim.dx,
                minimized ? 0 : max(0, this->dim.dy - titleHeight)};
    }

public:
    using Panel<WIDTH, HEIGHT, false>::Panel;
    void setTitle(const char* value)
    {
        strlcpy(title, value ? value : "", sizeof(title));
        this->invalidate();
    }
    void setTheme(const PanelTheme& value)
    {
        theme = value;
        theme.padding = max(0, theme.padding);
        theme.textHeight = max(1, theme.textHeight);
        this->setBackground(theme.background);
        this->invalidate();
    }
    void setFeatures(bool movable, bool resizable, bool closable)
    {
        moveEnabled = movable;
        resizeEnabled = resizable;
        closeEnabled = closable;
        this->invalidate();
    }
    void setMinSize(int w, int h) { minWidth = max(1, w); minHeight = max(titleHeight, h); }
    void setRaiseOnPress(bool value) { raiseOnPress = value; }
    void setSnapDistance(int pixels) { snapDistance = max(0, pixels); }
    bool isSnapped() const { return snap != Snap::NONE; }
    void fitSingleChild(bool value) { fitClient = value; reflowChildren(); }
    void registryChanged() override
    {
        Panel<WIDTH, HEIGHT, false>::registryChanged();
        parentViewportChanged();
    }
    void parentViewportChanged() override
    {
        if (isSnapped() && this->registry && this->registry->ownerComponent())
            setGeometry(snappedGeometry(this->registry->visibleArea()));
    }
    void setLayoutGeometry(PixmapRect rect) override
    {
        if (!isSnapped()) setGeometry(rect);
    }
    bool isMinimized() const { return minimized; }
    bool isMaximized() const { return snap == Snap::FULL; }
    void onClose(void (*callback)()) { closed = callback; }
    void setModal(bool value)
    {
        if (!this->registry) return;
        if (value) this->registry->setModal(this);
        else this->registry->clearModal(this);
    }
    void close()
    {
        this->setVisible(false);
        if (closed) closed();
    }
    void setMinimized(bool value)
    {
        if (value == minimized || isMaximized()) return;
        minimized = value;
        if (value)
        {
            snap = Snap::NONE;
            openHeight = this->dim.dy;
            this->cancelInput();
        }
        this->setSize(this->dim.dx, value ? titleHeight : openHeight);
        this->invalidate();
    }
    void setMaximized(bool value)
    {
        if (value == isMaximized() || !this->registry || !this->registry->ownerComponent()) return;
        if (value)
        {
            if (minimized) setMinimized(false);
            if (!isSnapped()) restore = this->bounds();
            snap = Snap::FULL;
            parentViewportChanged();
        }
        else
        {
            snap = Snap::NONE;
            setGeometry(restore);
        }
        this->invalidate();
    }
    void cancelInteraction() override
    {
        gesture = Gesture::NONE;
        dragged = false;
        Panel<WIDTH, HEIGHT, false>::cancelInteraction();
    }
    void handleEvent(Event e) override
    {
        if (!this->isInteractive()) return;
        const auto box = this->bounds();
        const int64_t localX = int64_t(e.startX) - box.x;
        const int64_t localY = int64_t(e.startY) - box.y;
        if (e.type == EventType::PRESS)
        {
            dragged = false;
            if (raiseOnPress && this->registry) this->registry->raise(this);
            const bool titleBar = localY >= 0 && localY < titleHeight;
            const bool buttons = localX >= box.width - 3 * buttonWidth();
            if (moveEnabled && titleBar && !buttons)
            {
                gesture = Gesture::MOVE;
                moveOrigin = isSnapped() ? restore : box;
            }
            else if (resizeEnabled && !isSnapped() && !minimized &&
                     localX >= box.width - 12 && localY >= box.height - 12)
                gesture = Gesture::RESIZE;
            else gesture = Gesture::NONE;
            if (gesture != Gesture::NONE) { this->cancelInput(); return; }
        }
        if (gesture != Gesture::NONE && (e.type == EventType::DRAG || e.type == EventType::RELEASE))
        {
            if (e.type == EventType::DRAG)
            {
                dragged = true;
                const auto dx = int64_t(e.endX) - e.startX, dy = int64_t(e.endY) - e.startY;
                if (gesture == Gesture::MOVE)
                {
                    if (isSnapped())
                    {
                        const int64_t anchor = max(int64_t(0), min(int64_t(box.width), localX));
                        const int64_t restoredAnchor = anchor * restore.width / max(1, box.width);
                        snap = Snap::NONE;
                        setGeometry({panelCoordinate(int64_t(e.endX) - restoredAnchor),
                                     panelCoordinate(int64_t(e.endY) - max(int64_t(0), localY)),
                                     restore.width, restore.height});
                    }
                    else this->setLoc(panelCoordinate(int64_t(box.x) + dx),
                                      panelCoordinate(int64_t(box.y) + dy));
                }
                else
                    this->setSize(max(minWidth, panelCoordinate(int64_t(box.width) + dx)),
                                  max(minHeight, panelCoordinate(int64_t(box.height) + dy)));
            }
            else
            {
                const bool shouldSnap = dragged && gesture == Gesture::MOVE && resizeEnabled;
                gesture = Gesture::NONE;
                dragged = false;
                if (shouldSnap)
                {
                    snap = snapTarget(e.endX, e.endY);
                    if (isSnapped())
                    {
                        restore = moveOrigin;
                        parentViewportChanged();
                    }
                }
            }
            return;
        }
        if ((e.type == EventType::CLICK || e.type == EventType::DOUBLE_CLICK) &&
            this->inBounds(e.endX, e.endY) && localY >= 0 && localY < titleHeight && buttonWidth())
        {
            const int64_t fromRight = int64_t(box.x) + box.width - 1 - e.endX;
            switch (fromRight / buttonWidth())
            {
                case 0: if (closeEnabled) close(); return;
                case 1: if (resizeEnabled) setMaximized(!isMaximized()); return;
                case 2: setMinimized(!minimized); return;
                default: if (e.type == EventType::DOUBLE_CLICK && resizeEnabled)
                             setMaximized(!isMaximized());
                         return;
            }
        }
        Panel<WIDTH, HEIGHT, false>::handleEvent(e);
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        Panel<WIDTH, HEIGHT, false>::render(canvas, x, y);
        const auto box = panelTranslate(this->bounds(), x, y);
        auto chrome = canvas;
        chrome.clip = panelIntersection(canvas.clip, box);
        if (!chrome.clip.width || !chrome.clip.height) return;
        const int bw = buttonWidth();
        const int height = min(titleHeight, box.height);
        chrome.fill({box.x, box.y, box.width, height}, theme.surface);
        const int padding = min(theme.padding, box.width / 2);
        chrome.text({box.x + padding, box.y + max(0, (height - theme.textHeight) / 2),
                     max(0, box.width - padding - 3 * bw), height},
                    title, theme.font, theme.text, theme.textHeight, theme.fontFilter);
        for (int i = 0; i < 3 && bw >= 8 && height >= 8; ++i)
        {
            const int bx = box.x + box.width - (i + 1) * bw;
            chrome.fill({bx, box.y, bw - 1, height}, i == 0 ? theme.border : theme.background);
            const int px = bx + (bw - 7) / 2, py = box.y + (height - 7) / 2;
            if (i == 0 && closeEnabled)
                for (int n = 0; n < 7; ++n)
                {
                    chrome.fill({px + n, py + n, 1, 1}, theme.text);
                    chrome.fill({px + n, py + 6 - n, 1, 1}, theme.text);
                }
            else if (i == 1 && resizeEnabled)
            {
                chrome.fill({px, py, 7, 1}, theme.text);
                chrome.fill({px, py + 6, 7, 1}, theme.text);
                chrome.fill({px, py, 1, 7}, theme.text);
                chrome.fill({px + 6, py, 1, 7}, theme.text);
            }
            else if (i == 2) chrome.fill({px, py + 5, 7, 1}, theme.text);
        }
        if (resizeEnabled && !minimized && !isSnapped())
        {
            const int right = box.x + box.width, bottom = box.y + box.height;
            for (int i = 2; i <= 6; i += 2)
                chrome.fill({right - i, bottom - 1, 1, 1}, theme.text);
        }
    }
};
