#pragma once
#include "Component.h"

// Lightweight widgets render directly to the Panel canvas.
class PanelProgress : public BaseComponent
{
protected:
    float value = 0;
    uint16_t track = RGB565(37, 52, 69);
    uint16_t accent = CYAN;

    int filledWidth() const { return int(value * dim.dx + 0.5f); }

public:
    PanelProgress(int width, int height, ComponentRegistry* parent = nullptr) : BaseComponent(parent)
    {
        dim = {max(0, width), max(0, height)};
    }

    void setValue(float next)
    {
        if (!isfinite(next)) return;
        next = max(0.0f, min(1.0f, next));
        if (next != value) { value = next; invalidate(); }
    }
    float getValue() const { return value; }
    void setColors(uint16_t background, uint16_t fill)
    {
        track = background;
        accent = fill;
        invalidate();
    }
    bool isInteractive() const override { return false; }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        const PixmapRect rect{x + ulhc.x, y + ulhc.y, dim.dx, dim.dy};
        canvas.fill(rect, track);
        canvas.fill({rect.x, rect.y, filledWidth(), rect.height}, accent);
    }
};

class PanelSlider : public PanelProgress
{
    void (*changed)(float) = nullptr;
    float lower = 0, upper = 1, step = 0;

public:
    using PanelProgress::PanelProgress;
    bool isInteractive() const override { return BaseComponent::isInteractive(); }
    void onChange(void (*callback)(float)) { changed = callback; }
    float getValue() const { return lower + value * (upper - lower); }
    bool setRange(float minimum, float maximum)
    {
        if (!isfinite(minimum) || !isfinite(maximum) || !(maximum > minimum) ||
            !isfinite(maximum - minimum)) return false;
        lower = minimum;
        upper = maximum;
        return true;
    }
    void setStep(float increment) { step = isfinite(increment) && increment > 0 ? increment : 0; }
    void setValue(float next)
    {
        if (!isfinite(next)) return;
        next = max(lower, min(upper, next));
        if (step > 0 && next > lower && next < upper)
        {
            const double ticks = round((double(next) - lower) / step);
            if (isfinite(ticks)) next = float(min(double(upper), lower + ticks * step));
        }
        PanelProgress::setValue((next - lower) / (upper - lower));
    }
    void handleEvent(Event e) override
    {
        if (e.type != EventType::PRESS && e.type != EventType::DRAG && e.type != EventType::RELEASE)
            return;
        const float previous = value;
        const float fraction = max(0.0f, min(1.0f, (float(e.endX) - ulhc.x) / max(1, dim.dx - 1)));
        setValue(lower + fraction * (upper - lower));
        if (changed && previous != value) changed(getValue());
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        PanelProgress::render(canvas, x, y);
        if (dim.dx <= 0 || dim.dy <= 0) return;
        const int center = int(value * (dim.dx - 1) + 0.5f);
        const int left = max(0, center - 2);
        canvas.fill({x + ulhc.x + left, y + ulhc.y, min(5, dim.dx - left), dim.dy}, WHITE);
    }
};

class PanelToggle : public PanelProgress
{
    void (*changed)(bool) = nullptr;

public:
    using PanelProgress::PanelProgress;
    bool isInteractive() const override { return BaseComponent::isInteractive(); }
    void setChecked(bool checked) { setValue(checked ? 1.0f : 0.0f); }
    bool isChecked() const { return value >= 0.5f; }
    void onChange(void (*callback)(bool)) { changed = callback; }
    void handleEvent(Event e) override
    {
        if ((e.type != EventType::CLICK && e.type != EventType::DOUBLE_CLICK) ||
            !inBounds(e.endX, e.endY)) return;
        setChecked(!isChecked());
        if (changed) changed(isChecked());
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        PanelProgress::render(canvas, x, y);
        if (dim.dx < 3 || dim.dy < 3) return;
        const int thumb = min(dim.dy - 4, dim.dx - 4);
        if (thumb > 0)
            canvas.fill({x + ulhc.x + (isChecked() ? dim.dx - thumb - 2 : 2),
                         y + ulhc.y + 2, thumb, dim.dy - 4}, WHITE);
    }
};

template <size_t WIDTH, size_t HEIGHT>
class PanelLabel : public Component<WIDTH, HEIGHT>
{
    const BitmapFont* font = nullptr;
    char text[80]{};
    int fontHeight = 0;
    uint16_t color = WHITE;

public:
    using Component<WIDTH, HEIGHT>::Component;
    bool isInteractive() const override { return false; }
    PanelLabel& setText(const char* value)
    {
        const char* next = value ? value : "";
        if (strncmp(text, next, sizeof(text) - 1) != 0)
        {
            strlcpy(text, next, sizeof(text));
            this->invalidate();
        }
        return *this;
    }
    PanelLabel& setFont(const BitmapFont& value, int height = 0)
    {
        font = &value;
        fontHeight = height;
        this->invalidate();
        return *this;
    }
    PanelLabel& setColor(uint16_t value) { color = value; this->invalidate(); return *this; }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        if (this->renderDirty)
        {
            this->pixmap.clear();
            if (font) this->pixmap.print(0, 0, text, *font, color, fontHeight);
        }
        Component<WIDTH, HEIGHT>::render(canvas, x, y);
    }
};

template <size_t WIDTH, size_t HEIGHT>
class PanelList : public Component<WIDTH, HEIGHT>
{
    const char* const* items = nullptr;
    int count = 0, selected = -1, offset = 0;
    int rowHeight = 22;
    const BitmapFont* font = nullptr;
    void (*changed)(int) = nullptr;

    void scrollBy(int64_t pixels)
    {
        const int64_t maxOffset = max(int64_t(0), int64_t(count) * rowHeight - int(HEIGHT));
        const int next = int(max(int64_t(0), min(min(int64_t(INT_MAX), maxOffset),
                                                 int64_t(offset) + pixels)));
        if (next != offset) { offset = next; this->invalidate(); }
    }

public:
    using Component<WIDTH, HEIGHT>::Component;
    void setItems(const char* const* values, int size)
    {
        items = values;
        count = items ? max(0, size) : 0;
        selected = -1;
        offset = 0;
        this->invalidate();
    }

    void setFont(const BitmapFont& value, int rowPixels = 22)
    {
        font = &value;
        rowHeight = max(4, rowPixels);
        scrollBy(0);
        this->invalidate();
    }
    int selectedIndex() const { return selected; }
    void onChange(void (*callback)(int)) { changed = callback; }

    void handleEvent(Event e) override
    {
        if (e.type == EventType::DRAG)
            scrollBy(int64_t(e.startY) - e.endY);
        else if (e.type == EventType::SCROLL && isfinite(e.direction) && e.direction != 0)
            scrollBy(e.direction > 0 ? int64_t(rowHeight) : -int64_t(rowHeight));
        else if ((e.type == EventType::CLICK || e.type == EventType::DOUBLE_CLICK) &&
                 this->inBounds(e.endX, e.endY))
        {
            const int64_t row = (int64_t(e.endY) - this->ulhc.y + offset) / rowHeight;
            if (row < count && row != selected)
            {
                selected = int(row);
                this->invalidate();
                if (changed) changed(selected);
            }
        }
    }

    void render(const PanelCanvas& canvas, int x, int y) override
    {
        if (this->renderDirty)
        {
            auto& image = this->pixmap;
            image.clear();
            image.draw_rect(0, 0, WIDTH, HEIGHT, 0, true, RGB565(20, 32, 48));
            int y0 = -(offset % rowHeight);
            for (int row = offset / rowHeight; row < count && y0 < int(HEIGHT); ++row)
            {
                image.draw_rect(0, y0, WIDTH, rowHeight - 1, 0, true,
                                row == selected ? RGB565(30, 110, 126) : RGB565(31, 47, 64));
                if (font && items[row])
                    image.print(5, y0 + 2, items[row], *font, WHITE, rowHeight - 4);
                if (int64_t(y0) + rowHeight >= int(HEIGHT))
                    break;
                y0 += rowHeight;
            }
        }
        Component<WIDTH, HEIGHT>::render(canvas, x, y);
    }
};

template <size_t WIDTH, size_t HEIGHT, size_t SAMPLES, bool BUFFERED = true>
class PanelPlot : public Component<BUFFERED ? WIDTH : 1, BUFFERED ? HEIGHT : 1>
{
    using Base = Component<BUFFERED ? WIDTH : 1, BUFFERED ? HEIGHT : 1>;
    static_assert(SAMPLES >= 2 && SAMPLES <= SIZE_MAX / 2, "Plot needs at least two samples");
    float samples[SAMPLES]{};
    size_t next = 0, count = 0;
    uint16_t background = RGB565(20, 32, 48);
    uint16_t line = CYAN;
    float minimum = 0, maximum = 1;
    size_t visibleSamples = SAMPLES;

    static constexpr uint16_t gridColor() { return RGB565(44, 60, 76); }

public:
    explicit PanelPlot(ComponentRegistry* parent = nullptr) : Base(parent)
    {
        this->dim = {int(WIDTH), int(HEIGHT)};
    }
    void setSize(int w, int h) override
    {
        if (BUFFERED) Base::setSize(w, h);
        else BaseComponent::setSize(w, h);
    }
    bool isInteractive() const override { return false; }
    void append(float reading)
    {
        if (!isfinite(reading)) return;
        samples[next] = reading;
        next = (next + 1) % SAMPLES;
        if (count < SAMPLES) ++count;
        this->invalidate();
    }
    void clearSamples() { next = count = 0; this->invalidate(); }
    bool setRange(float low, float high)
    {
        if (!isfinite(low) || !isfinite(high) || !(high > low) || !isfinite(high - low)) return false;
        minimum = low;
        maximum = high;
        this->invalidate();
        return true;
    }
    void setVisibleSamples(size_t samplesToShow)
    {
        visibleSamples = max(size_t(2), min(SAMPLES, samplesToShow));
        this->invalidate();
    }
    void setColors(uint16_t bg, uint16_t stroke)
    {
        background = bg;
        line = stroke;
        this->invalidate();
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        if (!BUFFERED || this->renderDirty)
        {
            const int width = this->dim.dx, height = this->dim.dy;
            if (!width || !height) return;
            const auto bounds = panelTranslate(this->bounds(), x, y);
            auto& pixmap = this->pixmap;
            if (BUFFERED)
            {
                pixmap.clear();
                pixmap.draw_rect(0, 0, width, height, 0, true, background);
                for (int i = 1; i < 4; ++i)
                    pixmap.draw_rect(0, height / 4 * i, width, 1, 0, true, gridColor());
            }
            else
            {
                canvas.fill(bounds, background);
                for (int i = 1; i < 4; ++i)
                    canvas.fill({bounds.x, bounds.y + height / 4 * i, width, 1}, gridColor());
            }
            float points[SAMPLES * 2];
            const size_t shown = min(count, visibleSamples);
            size_t index = (next + SAMPLES - shown) % SAMPLES;
            for (size_t i = 0; i < shown; ++i)
            {
                const float sample = max(minimum, min(maximum, samples[index]));
                const float normalized = (sample - minimum) / (maximum - minimum);
                points[2 * i] = 0.5f + float(i) * (width - 1) / (visibleSamples - 1);
                points[2 * i + 1] = 0.5f + (1.0f - normalized) * (height - 1);
                index = (index + 1) % SAMPLES;
            }
            if (shown >= 2)
            {
                StrokeStyle style;
                style.width = 1.5f;
                style.join = StrokeJoin::ROUND;
                style.cap = StrokeCap::ROUND;
                style.antialias = Antialiasing::COVERAGE;
                if (BUFFERED) pixmap.strokePath(points, shown, false, style, line);
                else
                {
                    const PixmapRect visible = panelIntersection(canvas.clip, bounds);
                    struct Paint { const PanelCanvas& canvas; PixmapRect bounds; uint16_t bg, ink; };
                    Paint paint{canvas, bounds, background, line};
                    pixelplanner_detail::strokePath(points, shown, false, style,
                        {visible.x - bounds.x, visible.y - bounds.y, visible.width, visible.height}, &paint,
                        [](void* context, int px, int py, uint8_t coverage) {
                            const auto& target = *static_cast<Paint*>(context);
                            const int step = target.bounds.height / 4;
                            const bool grid = py == step || py == step * 2 || py == step * 3;
                            const uint16_t color = blend565(grid ? gridColor() : target.bg, target.ink,
                                                           coverage / 255.0f);
                            target.canvas.fill({target.bounds.x + px, target.bounds.y + py, 1, 1}, color);
                        });
                }
            }
        }
        if (BUFFERED) Base::render(canvas, x, y);
        else this->renderDirty = false;
    }
};

template <size_t WIDTH, size_t HEIGHT, size_t ATLAS_W, size_t ATLAS_H>
class PanelSprite : public Component<WIDTH, HEIGHT>
{
    const Pixmap<ATLAS_W, ATLAS_H>& atlas;
    PixmapRect source;
    BlitTransform pose{WIDTH * 0.5f, HEIGHT * 0.5f};

public:
    PanelSprite(const Pixmap<ATLAS_W, ATLAS_H>& image, PixmapRect region,
                ComponentRegistry* parent = nullptr)
        : Component<WIDTH, HEIGHT>(parent), atlas(image), source(region) {}

    bool isInteractive() const override { return false; }
    void setPose(const BlitTransform& value) { pose = value; this->invalidate(); }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        if (this->renderDirty)
        {
            this->pixmap.clear();
            this->pixmap.blitTransformed(atlas, source, pose, ScaleMode::BILINEAR);
        }
        Component<WIDTH, HEIGHT>::render(canvas, x, y);
    }
};

enum class PanelAxis { HORIZONTAL, VERTICAL };

// Non-owning linear layout. All items must share the same parent coordinates.
inline void panelStack(BaseComponent* const* items, size_t count, int x, int y,
                       PanelAxis axis = PanelAxis::VERTICAL, int gap = 0)
{
    if (!items) return;
    for (size_t i = 0; i < count; ++i)
    {
        auto* item = items[i];
        if (!item) continue;
        item->setLoc(x, y);
        const auto size = item->getSize();
        if (axis == PanelAxis::VERTICAL) y += size.dy + gap;
        else x += size.dx + gap;
    }
}
