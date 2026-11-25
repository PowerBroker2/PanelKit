#pragma once
#include "Component.h"

// Scanline image view. Source storage is borrowed and must outlive the view.
class PanelPicture : public BaseComponent
{
    const uint16_t* pixels = nullptr;
    const bool* mask = nullptr;
    int sourceWidth = 0, sourceHeight = 0;
    bool flash = false;
    BlitMode mode = BlitMode::FIT;

public:
    PanelPicture(int w, int h, ComponentRegistry* parent = nullptr) : BaseComponent(parent)
    {
        dim = {max(0,w), max(0,h)};
    }
    bool isInteractive() const override { return false; }
    void setSource(const uint16_t* data, int w, int h, bool inFlash = true,
                   const bool* opacity = nullptr)
    {
        pixels = data;
        mask = opacity;
        sourceWidth = w;
        sourceHeight = h;
        flash = inFlash;
        invalidate();
    }
    template <size_t W, size_t H>
    void setPixmap(const Pixmap<W,H>& image)
    {
        setSource(image.getPixels(), W, H, false, image.getMask());
    }
    void setMode(BlitMode next) { if (mode != next) { mode = next; invalidate(); } }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        renderDirty = false;
        if (!pixels || sourceWidth <= 0 || sourceHeight <= 0 ||
            size_t(sourceWidth) > SIZE_MAX / size_t(sourceHeight)) return;
        const auto box = panelTranslate(bounds(), x, y);
        auto view = canvas;
        view.clip = panelIntersection(canvas.clip, box);
        if (!view.clip.width || !view.clip.height) return;
        float width = sourceWidth, height = sourceHeight;
        float left = box.x, top = box.y;
        if (mode == BlitMode::STRETCH) { width = box.width; height = box.height; }
        else if (mode == BlitMode::FIT || mode == BlitMode::FILL)
        {
            const float sx = box.width / width, sy = box.height / height;
            const float factor = mode == BlitMode::FIT ? min(sx, sy) : max(sx, sy);
            width *= factor;
            height *= factor;
        }
        if (mode == BlitMode::CENTER || mode == BlitMode::FIT || mode == BlitMode::FILL)
        {
            left += (box.width - width) * 0.5f;
            top += (box.height - height) * 0.5f;
        }
        if (mode == BlitMode::TOP_RIGHT || mode == BlitMode::BOTTOM_RIGHT) left += box.width - width;
        if (mode == BlitMode::BOTTOM_LEFT || mode == BlitMode::BOTTOM_RIGHT) top += box.height - height;
        const int leftPixel = max(view.clip.x, panelCoordinate(int64_t(ceilf(left - 0.5f))));
        const int rightPixel = min(view.clip.x + view.clip.width, panelCoordinate(int64_t(ceilf(left + width - 0.5f))));
        constexpr int chunk = 48;
        uint16_t colors[chunk];
        bool visible[chunk];
        const float sx = sourceWidth / width, sy = sourceHeight / height;
        for (int row = view.clip.y; row < view.clip.y + view.clip.height; ++row)
        {
            const float v = row + 0.5f - top;
            if (v < 0 || v >= height) continue;
            const size_t srcRow = size_t(min(int64_t(sourceHeight - 1), int64_t(v * sy))) * sourceWidth;
            for (int col = leftPixel; col < rightPixel;)
            {
                const int count = min(chunk, rightPixel - col);
                for (int i = 0; i < count; ++i)
                {
                    const float u = col + i + 0.5f - left;
                    const size_t source = srcRow + max(int64_t(0),
                        min(int64_t(sourceWidth - 1), int64_t(u * sx)));
                    colors[i] = flash ? pgm_read_word(pixels + source) : pixels[source];
                    visible[i] = !mask || (flash ? pgm_read_byte(mask + source) != 0 : mask[source]);
                }
                view.blit({col, row, count, 1}, colors, mask ? visible : nullptr);
                col += count;
            }
        }
    }
};
