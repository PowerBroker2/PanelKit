#pragma once
#include <Arduino.h>
#include "Component.h"
#include "Events.h"
#include "PixelPlanner.h"

// Text Justification & Vertical Alignment
enum class TextJustify { LEFT, CENTER, RIGHT };
enum class TextVAlign  { TOP, MIDDLE, BOTTOM };

template <uint16_t WIDTH, uint16_t HEIGHT, bool BUFFERED_TEXT = true>
class PanelButton : public BaseComponent
{
private:
    char label[32] = "";
    const BitmapFont* font = nullptr;
    float fontScale = 1.0f;
    TextJustify hJustify = TextJustify::CENTER;
    TextVAlign  vJustify = TextVAlign::MIDDLE;
    int padding = 4;

    uint16_t bgColor        = 0x4208; // Dark Gray
    uint16_t textColor      = 0xFFFF; // White
    uint16_t pressedBgColor = 0x2104; // Darker Gray
    uint16_t borderColor    = 0x7BEF; // Light Gray
    uint16_t cornerRadius   = 0;      // Corner radius for rounded rects

    bool isPressed = false;
    bool trackingPress = false;
    bool subscribed = false;
    bool dirty     = true;
    bool needsBlit = true;            // Flags when hardware update is required

    void (*clickCallback)() = nullptr;
    void (*doubleClickCallback)() = nullptr;

    // Slot required by tomstewart89/Callback library
    MethodSlot<PanelButton, Event> eventSlot;

    void receiveGlobal(Event e)
    {
        if (!this->registry) onEvent(e);
    }

    int calculateTextX(int textWidth) const
    {
        switch (hJustify)
        {
            case TextJustify::LEFT:   return padding;
            case TextJustify::RIGHT:  return int(WIDTH) - textWidth - padding;
            case TextJustify::CENTER:
            default:                  return (int(WIDTH) - textWidth) / 2;
        }
    }

    int calculateTextY(int textHeight) const
    {
        switch (vJustify)
        {
            case TextVAlign::TOP:    return padding;
            case TextVAlign::BOTTOM: return int(HEIGHT) - textHeight - padding;
            case TextVAlign::MIDDLE:
            default:                 return (int(HEIGHT) - textHeight) / 2;
        }
    }

public:
    // Off-screen pixel buffers managed by PixelPlanner
    Pixmap<WIDTH, HEIGHT> pixmap;
    Pixmap<BUFFERED_TEXT ? WIDTH : 1, BUFFERED_TEXT ? HEIGHT : 1> textPixmap;

    explicit PanelButton(ComponentRegistry* parent = nullptr)
        : BaseComponent(parent), eventSlot(this, &PanelButton::receiveGlobal)
    {
        // Set dimensions on the base class struct so Panel's inBounds() works
        this->dim.dx = WIDTH;
        this->dim.dy = HEIGHT;

        registryChanged();
    }

    ~PanelButton()
    {
        if (subscribed) getEventSignal().detach(eventSlot);
    }

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

    // --- Fluent Setters ---

    PanelButton& setText(const char* txt)
    {
        strncpy(this->label, txt ? txt : "", sizeof(this->label) - 1);
        this->label[sizeof(this->label) - 1] = '\0';
        dirty = true;
        this->invalidate();
        return *this;
    }

    PanelButton& setFont(const BitmapFont& f, float scale = 1.0f)
    {
        this->font = &f;
        this->fontScale = scale;
        dirty = true;
        this->invalidate();
        return *this;
    }

    PanelButton& setJustification(TextJustify h, TextVAlign v = TextVAlign::MIDDLE)
    {
        this->hJustify = h;
        this->vJustify = v;
        dirty = true;
        this->invalidate();
        return *this;
    }

    PanelButton& setColors(uint16_t bg, uint16_t text, uint16_t pressedBg = 0, uint16_t border = 0)
    {
        this->bgColor = bg;
        this->textColor = text;
        this->pressedBgColor = (pressedBg != 0) ? pressedBg : bg;
        this->borderColor = (border != 0) ? border : bg;
        dirty = true;
        this->invalidate();
        return *this;
    }

    PanelButton& setCornerRadius(uint16_t radius)
    {
        this->cornerRadius = radius;
        dirty = true;
        this->invalidate();
        return *this;
    }

    PanelButton& onClick(void (*cb)())
    {
        this->clickCallback = cb;
        return *this;
    }

    PanelButton& onDoubleClick(void (*callback)())
    {
        doubleClickCallback = callback;
        return *this;
    }

    void redraw()
    {
        const uint16_t bg = isPressed ? pressedBgColor : bgColor;
        const int radius = min(int(cornerRadius), min(int(WIDTH), int(HEIGHT)) / 2);
        pixmap.clear();
        for (int y = 0; y < int(HEIGHT); ++y)
        {
            const int distance = min(y, int(HEIGHT) - 1 - y);
            const int dy = max(0, radius - distance);
            const int inset = radius - int(sqrtf(float(int64_t(radius) * radius - int64_t(dy) * dy)));
            if (int(WIDTH) <= 2 * inset)
                continue;
            const bool edge = y == 0 || y == int(HEIGHT) - 1;
            pixmap.draw_rect(inset, y, int(WIDTH) - 2 * inset, 1, 0, true, borderColor);
            if (!edge && int(WIDTH) > 2 * inset + 2)
                pixmap.draw_rect(inset + 1, y, int(WIDTH) - 2 * inset - 2, 1, 0, true, bg);
        }

        textPixmap.clear();
        if (label[0] && font && font->getHeight() > 0 && isfinite(fontScale) && fontScale > 0)
        {
            const int contentHeight = max(1, int(HEIGHT) - padding * 2);
            const int height = int(max(1.0f, min(float(contentHeight), roundf(font->getHeight() * fontScale))));
            int64_t textWidth = 0;
            for (const char* c = label; *c; ++c)
            {
                const auto glyph = font->lookupChar(*c);
                if (glyph.data && glyph.width > 0)
                    textWidth += max(int64_t(1),
                        (int64_t(glyph.width) * height + font->getHeight() / 2) / font->getHeight());
                else if (*c == ' ')
                    textWidth += height / 3;
            }
            const int width = int(min(int64_t(INT_MAX / 2), textWidth));
            if (BUFFERED_TEXT)
            {
                drawCaption(textPixmap, width, height);
                pixmap.blit_from_pixmap(textPixmap);
            }
            else drawCaption(pixmap, width, height);
        }
        dirty = false;
        needsBlit = true;
    }

    void render(const PanelCanvas& canvas, int x, int y) override
    {
        update();
        const auto loc = this->getLoc();
        canvas.blit({x + loc.x, y + loc.y, int(WIDTH), int(HEIGHT)},
                    pixmap.getPixels(), pixmap.getMask());
    }

    template <typename TFT>
    void drawTo(TFT& display, bool force = false)
    {
        update();
        if (!needsBlit && !this->renderDirty && !force) return;
        if (this->isVisible()) render(PanelCanvas::from(display), 0, 0);
        needsBlit = this->renderDirty = false;
    }

    void handleEvent(Event e) override { onEvent(e); }
    void deactivate() override { cancelInteraction(); BaseComponent::deactivate(); }
    void cancelInteraction() override
    {
        trackingPress = false;
        setPressed(false);
    }

    void onEvent(Event e)
    {
        if (!this->isInteractive()) return;
        switch (e.type)
        {
            case EventType::PRESS:
                trackingPress = this->inBounds(e.startX, e.startY);
                setPressed(trackingPress);
                break;
            case EventType::DRAG:
                setPressed(trackingPress && this->inBounds(e.endX, e.endY));
                break;
            case EventType::RELEASE:
                cancelInteraction();
                break;
            case EventType::CLICK:
                if (this->inBounds(e.startX, e.startY) && this->inBounds(e.endX, e.endY) && clickCallback)
                    clickCallback();
                break;
            case EventType::DOUBLE_CLICK:
                if (this->inBounds(e.startX, e.startY) && this->inBounds(e.endX, e.endY))
                {
                    const auto callback = doubleClickCallback ? doubleClickCallback : clickCallback;
                    if (callback) callback();
                }
                break;
            default:
                break;
        }
    }

    void update()
    {
        if (dirty) redraw();
    }

private:
    template <size_t W, size_t H>
    void drawCaption(Pixmap<W, H>& target, int width, int height)
    {
        target.setClipRect({padding, padding, max(0, int(WIDTH) - padding * 2),
                            max(0, int(HEIGHT) - padding * 2)});
        target.print(calculateTextX(width) + isPressed, calculateTextY(height) + isPressed,
                      label, *font, textColor, height);
        target.resetClipRect();
    }

    void setPressed(bool value)
    {
        if (isPressed != value)
        {
            isPressed = value;
            dirty = true;
            this->invalidate();
        }
    }
};
