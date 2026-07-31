#pragma once
#include <Arduino.h>
#include "Component.h"
#include "Events.h"
#include "PixelPlanner.h"

// Text Justification & Vertical Alignment
enum class TextJustify { LEFT, CENTER, RIGHT };
enum class TextVAlign  { TOP, MIDDLE, BOTTOM };

template <uint16_t WIDTH, uint16_t HEIGHT>
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
    bool dirty     = true;
    bool needsBlit = true;            // Flags when hardware update is required

    void (*clickCallback)() = nullptr;

    // Slot required by tomstewart89/Callback library
    MethodSlot<PanelButton<WIDTH, HEIGHT>, Event> eventSlot;

    int calculateTextX(int textWidth) const
    {
        switch (hJustify)
        {
            case TextJustify::LEFT:   return padding;
            case TextJustify::RIGHT:  return WIDTH - textWidth - padding;
            case TextJustify::CENTER: 
            default:                  return (WIDTH - textWidth) / 2;
        }
    }

    int calculateTextY(int textHeight) const
    {
        switch (vJustify)
        {
            case TextVAlign::TOP:    return padding;
            case TextVAlign::BOTTOM: return HEIGHT - textHeight - padding;
            case TextVAlign::MIDDLE: 
            default:                 return (HEIGHT - textHeight) / 2;
        }
    }

public:
    // Off-screen pixel buffer managed by PixelPlanner
    Pixmap<WIDTH, HEIGHT> pixmap;

    PanelButton()
        : eventSlot(this, &PanelButton::onEvent)
    {
        // Set dimensions on the base class struct so Panel's inBounds() works
        this->dim.dx = WIDTH;
        this->dim.dy = HEIGHT;

        getEventSignal().attach(eventSlot);
    }

    ~PanelButton()
    {
        getEventSignal().detach(eventSlot);
    }

    // --- Fluent Setters ---

    PanelButton& setText(const char* txt)
    {
        strncpy(this->label, txt, sizeof(this->label) - 1);
        this->label[sizeof(this->label) - 1] = '\0';
        dirty = true;
        return *this;
    }

    PanelButton& setFont(const BitmapFont& f, float scale = 1.0f)
    {
        this->font = &f;
        this->fontScale = scale;
        dirty = true;
        return *this;
    }

    PanelButton& setJustification(TextJustify h, TextVAlign v = TextVAlign::MIDDLE)
    {
        this->hJustify = h;
        this->vJustify = v;
        dirty = true;
        return *this;
    }

    PanelButton& setColors(uint16_t bg, uint16_t text, uint16_t pressedBg = 0, uint16_t border = 0)
    {
        this->bgColor = bg;
        this->textColor = text;
        this->pressedBgColor = (pressedBg != 0) ? pressedBg : bg;
        this->borderColor = (border != 0) ? border : bg;
        dirty = true;
        return *this;
    }

    PanelButton& setCornerRadius(uint16_t radius)
    {
        this->cornerRadius = radius;
        dirty = true;
        return *this;
    }

    PanelButton& onClick(void (*cb)())
    {
        this->clickCallback = cb;
        return *this;
    }

    // --- Drawing Routine ---

    void redraw()
    {
        uint16_t currentBg = isPressed ? pressedBgColor : bgColor;

        // 1. Fill background (x, y, w, h, radius, fill, color, alpha)
        pixmap.draw_rect(0, 0, WIDTH, HEIGHT, cornerRadius, true, currentBg, 1.0f);

        // 2. Border outline
        if (borderColor != currentBg)
        {
            pixmap.draw_rect(0, 0, WIDTH, HEIGHT, cornerRadius, false, borderColor, 1.0f);
        }

        // 3. Print text
        if (label[0] != '\0' && font != nullptr)
        {
            int textWidth  = strlen(label) * 12 * fontScale; 
            int textHeight = 16 * fontScale;

            int tx = calculateTextX(textWidth);
            int ty = calculateTextY(textHeight);

            if (isPressed)
            {
                tx += 1;
                ty += 1;
            }

            pixmap.print(tx, ty, label, *font, textColor, 20, fontScale);
        }

        dirty = false;
        needsBlit = true; // Signal drawTo() to update display
    }

    // --- Hardware Blitting Helper for TeensyTFT ---

    template <typename TFT>
    void drawTo(TFT& display, bool force = false)
    {
        if (!needsBlit && !force) return;

        uint16_t bx = this->getLoc().x;
        uint16_t by = this->getLoc().y;

        // Render pixmap pixels directly to TeensyTFT hardware
        for (uint16_t py = 0; py < HEIGHT; py++)
        {
            for (uint16_t px = 0; px < WIDTH; px++)
            {
                display.drawPixel(bx + px, by + py, pixmap.getPixelValue(px, py));
            }
        }

        needsBlit = false;
    }

    // --- Touch Event Slot ---

    void onEvent(Event e)
    {
        if (!this->isActive()) return;

        bool startIn   = this->inBounds(e.startX, e.startY);
        bool currentIn = this->inBounds(e.endX, e.endY);

        switch (e.type)
        {
            case EventType::PRESS:
                if (startIn || currentIn)
                {
                    isPressed = true;
                    dirty = true;
                }
                break;

            case EventType::DRAG:
                if (isPressed && !currentIn)
                {
                    // Finger dragged outside button bounds -> revert state
                    isPressed = false;
                    dirty = true;
                }
                else if (!isPressed && startIn && currentIn)
                {
                    // Finger dragged back inside button bounds -> re-highlight
                    isPressed = true;
                    dirty = true;
                }
                break;

            case EventType::RELEASE:
                if (isPressed)
                {
                    isPressed = false;
                    dirty = true;
                }
                break;

            case EventType::CLICK:
                if (isPressed)
                {
                    isPressed = false;
                    dirty = true;
                }
                // Trigger callback only if touch both started AND ended inside button
                if (startIn && currentIn && clickCallback)
                {
                    clickCallback();
                }
                break;

            default:
                break;
        }
    }

    void update()
    {
        if (dirty)
        {
            redraw();
        }
    }
};