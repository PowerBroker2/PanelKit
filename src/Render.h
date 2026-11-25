#pragma once
#include <Arduino.h>
#include <PixelPlanner.h>

inline PixmapRect panelIntersection(PixmapRect a, PixmapRect b)
{
    const int x = max(a.x, b.x), y = max(a.y, b.y);
    const int64_t right = min(int64_t(a.x) + a.width, int64_t(b.x) + b.width);
    const int64_t bottom = min(int64_t(a.y) + a.height, int64_t(b.y) + b.height);
    return {x, y, right > x ? int(right - x) : 0, bottom > y ? int(bottom - y) : 0};
}

inline int panelCoordinate(int64_t value)
{
    return int(max(int64_t(INT_MIN), min(int64_t(INT_MAX), value)));
}

inline PixmapRect panelTranslate(PixmapRect rect, int x, int y)
{
    rect.x = panelCoordinate(int64_t(rect.x) + x);
    rect.y = panelCoordinate(int64_t(rect.y) + y);
    return rect;
}

inline PixmapRect panelUnion(PixmapRect a, PixmapRect b)
{
    if (!a.width || !a.height) return b;
    if (!b.width || !b.height) return a;
    const int x = min(a.x, b.x), y = min(a.y, b.y);
    return {x, y, panelCoordinate(max(int64_t(a.x) + a.width, int64_t(b.x) + b.width) - x),
                  panelCoordinate(max(int64_t(a.y) + a.height, int64_t(b.y) + b.height) - y)};
}

inline bool panelContains(PixmapRect rect, int x, int y)
{
    return x >= rect.x && y >= rect.y &&
           int64_t(x) - rect.x < rect.width && int64_t(y) - rect.y < rect.height;
}

inline int panelGlyphWidth(Glyph glyph, char code, int height, int originalHeight)
{
    if (!glyph.data || glyph.width <= 0)
        return code == ' ' ? height / 3 : 0;
    return panelCoordinate(max(int64_t(1),
        (int64_t(glyph.width) * height + originalHeight / 2) / originalHeight));
}

inline int panelTextWidth(const char* text, const BitmapFont& font, int height)
{
    if (!text || height <= 0 || font.getHeight() <= 0) return 0;
    int width = 0;
    for (; *text; ++text)
        width = panelCoordinate(int64_t(width) + panelGlyphWidth(font.lookupChar(*text), *text,
                                                               height, font.getHeight()));
    return width;
}

struct PanelCanvas
{
    void* device;
    PixmapRect clip;
    void (*paintRect)(void*, int, int, int, int, uint16_t);
    void (*paintPixels)(void*, int, int, int, int, const uint16_t*, const bool*);

    template <typename Display>
    static PanelCanvas from(Display& display)
    {
        return {&display, {0, 0, display.width(), display.height()},
            [](void* target, int x, int y, int w, int h, uint16_t color) {
                static_cast<Display*>(target)->fillRect(x, y, w, h, color);
            },
            [](void* target, int x, int y, int w, int h, const uint16_t* pixels, const bool* mask) {
                static_cast<Display*>(target)->fillRegion(x, y, w, h, pixels, mask);
            }};
    }

    void fill(PixmapRect rect, uint16_t color) const
    {
        rect = panelIntersection(rect, clip);
        if (rect.width && rect.height)
            paintRect(device, rect.x, rect.y, rect.width, rect.height, color);
    }

    void blit(PixmapRect bounds, const uint16_t* pixels, const bool* mask) const
    {
        const auto region = panelIntersection(bounds, clip);
        if (!region.width || !region.height)
            return;
        const size_t first = size_t(region.y - bounds.y) * bounds.width + region.x - bounds.x;
        if (region.width == bounds.width)
            paintPixels(device, region.x, region.y, region.width, region.height,
                        pixels + first, mask ? mask + first : nullptr);
        else
            for (int row = 0; row < region.height; ++row)
            {
                const size_t offset = first + size_t(row) * bounds.width;
                paintPixels(device, region.x, region.y + row, region.width, 1,
                            pixels + offset, mask ? mask + offset : nullptr);
            }
    }

    // Draw runs of glyph pixels directly into the clipped canvas.
    void text(PixmapRect bounds, const char* value, const BitmapFont* font,
              uint16_t color, int height, BitmapFilter filter = BitmapFilter::PRESERVE_STROKES) const
    {
        if (!value || !font || height <= 0 || font->getHeight() <= 0) return;
        const auto region = panelIntersection(bounds, clip);
        if (!region.width || !region.height) return;
        const int originalHeight = font->getHeight();
        int x = bounds.x;
        const int right = region.x + region.width, bottom = region.y + region.height;
        for (; *value && x < right; ++value)
        {
            const Glyph glyph = font->lookupChar(*value);
            const int width = panelGlyphWidth(glyph, *value, height, originalHeight);
            const int end = panelCoordinate(int64_t(x) + width);
            if (glyph.data && glyph.width > 0 && end > region.x)
            {
                const int first = max(x, region.x), last = min(end, right);
                for (int y = region.y; y < bottom && int64_t(y) - bounds.y < height; ++y)
                {
                    int run = first;
                    for (int col = first; col < last; ++col)
                    {
                        if (!sampleBitmapGlyph(glyph, originalHeight, width, height,
                                               col - x, y - bounds.y, filter))
                        {
                            if (run != col) paintRect(device, run, y, col - run, 1, color);
                            run = col + 1;
                        }
                    }
                    if (run < last) paintRect(device, run, y, last - run, 1, color);
                }
            }
            x = end;
        }
    }
};
