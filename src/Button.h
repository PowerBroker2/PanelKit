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
        if (!this->registry) this->dispatchInput(e);
    }

    int calculateTextX(int textWidth, PixmapRect area) const
    {
        switch (hJustify)
        {
            case TextJustify::LEFT:   return area.x;
            case TextJustify::RIGHT:  return area.x + area.width - textWidth;
            case TextJustify::CENTER:
            default:                  return area.x + (area.width - textWidth) / 2;
        }
    }

    int calculateTextY(int height, PixmapRect area) const
    {
        int first = height, last = 0;
        const int native = font->getHeight();
        for (const char* code = label; *code && (first > 0 || last < height); ++code)
        {
            const auto glyph = font->lookupChar(*code);
            if (!glyph.data || glyph.width <= 0) continue;
            const int width = panelGlyphWidth(glyph, *code, height, native);
            for (int y = 0; y < height; ++y)
            {
                if (y >= first && y < last) continue;
                for (int x = 0; x < width; ++x)
                    if (sampleBitmapGlyph(glyph, native, width, height, x, y))
                    {
                        first = min(first, y);
                        last = max(last, y + 1);
                        break;
                    }
            }
        }
        if (first >= last) return area.y;
        const int slack = area.height - (last - first);
        int offset = vJustify == TextVAlign::TOP ? 0 : vJustify == TextVAlign::BOTTOM ? slack : slack / 2;
        if (isPressed && offset < slack) ++offset;
        return area.y + offset - first;
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

    PanelButton& setPadding(int pixels)
    {
        padding = max(0, pixels);
        dirty = true;
        this->invalidate();
        return *this;
    }

    void setSize(int w, int h) override
    {
        const auto old = this->getSize();
        BaseComponent::setSize(min(w, int(WIDTH)), min(h, int(HEIGHT)));
        if (old.dx != this->dim.dx || old.dy != this->dim.dy) dirty = true;
    }

    void redraw()
    {
        const uint16_t bg = isPressed ? pressedBgColor : bgColor;
        const int radius = min(int(cornerRadius), min(this->dim.dx, this->dim.dy) / 2);
        pixmap.clear();
        for (int y = 0; y < this->dim.dy; ++y)
        {
            const int distance = min(y, this->dim.dy - 1 - y);
            const int dy = max(0, radius - distance);
            const int inset = radius - int(sqrtf(float(int64_t(radius) * radius - int64_t(dy) * dy)));
            if (this->dim.dx <= 2 * inset)
                continue;
            const bool edge = y == 0 || y == this->dim.dy - 1;
            pixmap.draw_rect(inset, y, this->dim.dx - 2 * inset, 1, 0, true, borderColor);
            if (!edge && this->dim.dx > 2 * inset + 2)
                pixmap.draw_rect(inset + 1, y, this->dim.dx - 2 * inset - 2, 1, 0, true, bg);
        }

        textPixmap.clear();
        const int px = min(padding, max(0, (this->dim.dx - 1) / 2));
        const int py = min(padding, max(0, (this->dim.dy - 1) / 2));
        const PixmapRect content{px, py, this->dim.dx - 2 * px, this->dim.dy - 2 * py};
        if (content.width && content.height && label[0] && font && font->getHeight() > 0 &&
            isfinite(fontScale) && fontScale > 0)
        {
            const int height = int(max(1.0f, min(float(content.height), roundf(font->getHeight() * fontScale))));
            const int width = panelTextWidth(label, *font, height);
            if (BUFFERED_TEXT)
            {
                drawCaption(textPixmap, content, width, height);
                pixmap.blit_from_pixmap(textPixmap);
            }
            else drawCaption(pixmap, content, width, height);
        }
        dirty = false;
        needsBlit = true;
    }

    void render(const PanelCanvas& canvas, int x, int y) override
    {
        update();
        auto clipped = canvas;
        const auto bounds = panelTranslate(this->bounds(), x, y);
        clipped.clip = panelIntersection(canvas.clip, bounds);
        clipped.blit({bounds.x, bounds.y, int(WIDTH), int(HEIGHT)},
                     pixmap.getPixels(), pixmap.getMask());
        this->renderDirty = false;
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
    void drawCaption(Pixmap<W, H>& target, PixmapRect content, int width, int height)
    {
        const int x = calculateTextX(width, content);
        const bool shift = isPressed && int64_t(x) + width < int64_t(content.x) + content.width;
        target.setClipRect(content);
        target.print(x + shift, calculateTextY(height, content), label, *font, textColor, height);
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
