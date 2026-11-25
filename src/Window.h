#pragma once
#include "Controls.h"

template <size_t WIDTH, size_t HEIGHT>
class PanelWindow : public Panel<WIDTH, HEIGHT, false>
{
    enum class Gesture { NONE, MOVE, RESIZE };
    Gesture gesture = Gesture::NONE;
    PanelTheme theme;
    char title[64]{};
    PixmapRect restore{};
    bool minimized = false, maximized = false;
    bool snapped = false, dragged = false;
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
        client->setLoc(0, 0);
        client->setSize(area.width, area.height);
    }

    void setGeometry(PixmapRect rect)
    {
        this->setLoc(rect.x, rect.y);
        this->setSize(rect.width, rect.height);
    }

    PixmapRect snapTarget(int x, int y) const
    {
        if (!snapDistance || !this->registry || !this->registry->ownerComponent() || minimized)
            return {};
        const auto area = this->registry->visibleArea();
        if (area.width <= 0 || area.height <= 0) return {};
        const int64_t right = int64_t(area.x) + area.width, bottom = int64_t(area.y) + area.height;
        // Captured drags can reach the physical screen edge outside a nested workspace.
        x = panelCoordinate(max(int64_t(area.x), min(right - 1, int64_t(x))));
        y = panelCoordinate(max(int64_t(area.y), min(bottom - 1, int64_t(y))));
        const bool leftEdge = x < int64_t(area.x) + snapDistance;
        const bool rightEdge = x >= right - snapDistance;
        const bool topEdge = y < int64_t(area.y) + snapDistance;
        const bool bottomEdge = y >= bottom - snapDistance;
        if (!leftEdge && !rightEdge) return topEdge ? area : PixmapRect{};
        const int halfWidth = area.width / 2, halfHeight = area.height / 2;
        const int sx = leftEdge ? area.x : area.x + halfWidth;
        const int sw = leftEdge ? halfWidth : area.width - halfWidth;
        if (topEdge) return {sx, area.y, sw, halfHeight};
        if (bottomEdge) return {sx, area.y + halfHeight, sw, area.height - halfHeight};
        return {sx, area.y, sw, area.height};
    }

    void applySnap(PixmapRect target)
    {
        if (!target.width || !target.height) return;
        const auto area = this->registry->visibleArea();
        restore = this->bounds();
        maximized = target.width == area.width && target.height == area.height;
        snapped = !maximized;
        setGeometry(target);
    }

protected:
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
    bool isSnapped() const { return snapped || maximized; }
    void fitSingleChild(bool value) { fitClient = value; layoutClient(); }
    void setSize(int w, int h) override
    {
        Panel<WIDTH, HEIGHT, false>::setSize(w, h);
        layoutClient();
    }
    bool isMinimized() const { return minimized; }
    bool isMaximized() const { return maximized; }
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
        if (value == minimized || maximized) return;
        minimized = value;
        if (value)
        {
            snapped = false;
            openHeight = this->dim.dy;
            this->cancelInput();
        }
        this->setSize(this->dim.dx, value ? titleHeight : openHeight);
        this->invalidate();
    }
    void setMaximized(bool value)
    {
        if (value == maximized || !this->registry || !this->registry->ownerComponent()) return;
        if (value)
        {
            if (minimized) setMinimized(false);
            if (!snapped) restore = this->bounds();
            setGeometry(this->registry->visibleArea());
        }
        else setGeometry(restore);
        maximized = value;
        snapped = false;
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
                gesture = Gesture::MOVE;
            else if (resizeEnabled && !maximized && !snapped && !minimized &&
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
                        maximized = snapped = false;
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
                if (shouldSnap) applySnap(snapTarget(e.endX, e.endY));
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
                case 1: if (resizeEnabled) setMaximized(!maximized); return;
                case 2: setMinimized(!minimized); return;
                default: if (e.type == EventType::DOUBLE_CLICK && resizeEnabled)
                             setMaximized(!maximized);
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
