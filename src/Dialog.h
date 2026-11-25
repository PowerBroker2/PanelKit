#pragma once
#include "Panel.h"
#include "Controls.h"

/**
 * A modal panel with a title bar and asynchronous results. Link widgets to the
 * dialog just as you would link them to a panel. Child coordinates start below
 * the title bar. Mount the dialog on the root panel to block the whole screen.
 */
template <size_t WIDTH, size_t HEIGHT>
class PanelDialog : public Panel<WIDTH, HEIGHT, false>
{
    using Base = Panel<WIDTH, HEIGHT, false>;
    enum : int { TitleHeight = 24 };
    PanelTheme theme;
    char title[64]{};
    bool opened = false;
    bool centerOnOpen = true;
    bool acceptOnEnter = false;

public:
    enum : int { Rejected = 0, Accepted = 1 };
    using ResultHandler = void (*)(int result, void* context);

private:
    ResultHandler finished = nullptr;
    void* context = nullptr;

    void center()
    {
        if (!this->registry || !this->registry->ownerComponent()) return;
        const auto area = this->registry->visibleArea();
        this->setLoc(panelCoordinate(int64_t(area.x) + max(0, (area.width - this->dim.dx) / 2)),
                     panelCoordinate(int64_t(area.y) + max(0, (area.height - this->dim.dy) / 2)));
    }

protected:
    PixmapRect contentRect() const override
    {
        const int top = min(int(TitleHeight), this->dim.dy);
        return {0, top, this->dim.dx, this->dim.dy - top};
    }

public:
    explicit PanelDialog(ComponentRegistry* parent = nullptr) : Base(parent)
    {
        Base::setVisible(false);
        this->setBackground(theme.background);
    }

    void setTitle(const char* value)
    {
        strlcpy(title, value ? value : "", sizeof(title));
        this->invalidate();
    }
    void setTheme(const PanelTheme& value)
    {
        theme = value;
        this->setBackground(theme.background);
        this->invalidate();
    }
    void setCenterOnOpen(bool value) { centerOnOpen = value; }
    void setAcceptOnEnter(bool value) { acceptOnEnter = value; }
    bool isOpen() const { return opened; }

    /** Called once per completion, after the dialog is hidden and input is released. */
    void onFinished(ResultHandler callback, void* userContext = nullptr)
    {
        finished = callback;
        context = userContext;
    }

    /**
     * Show the dialog and return immediately. An optional initial focus must
     * be an active, visible direct child. Returns false if already open, if
     * another modal owns the parent, or if the dialog or focus cannot be routed.
     */
    bool open(BaseComponent* initialFocus = nullptr)
    {
        auto* owner = this->registry;
        auto* parent = owner ? owner->ownerComponent() : nullptr;
        if (opened || this->overlay || !parent || !parent->isInteractive() || owner->activeModal() ||
            !this->setInputFocus(initialFocus)) return false;
        if (centerOnOpen) center();
        opened = true;
        Base::activate();
        Base::setVisible(true);
        owner->setModal(this);
        return true;
    }

    /** Complete with a standard or application-defined integer result. */
    bool finish(int result)
    {
        if (!opened) return false;
        opened = false;
        if (this->registry) this->registry->clearModal(this);
        this->dismissPopups();
        Base::setVisible(false);
        if (auto* child = this->activeModal()) child->setVisible(false);
        if (finished) finished(result, context);
        return true;
    }

    bool accept() { return finish(Accepted); }
    bool reject() { return finish(Rejected); }
    void setVisible(bool value) override
    {
        if (value) open();
        else if (opened) reject();
        else Base::setVisible(false);
    }
    void deactivate() override
    {
        Base::deactivate();
        if (opened) reject();
    }
    void registryChanged() override
    {
        Base::registryChanged();
        if (!this->registry && opened)
        {
            opened = false;
            this->dismissPopups();
            Base::setVisible(false);
            if (auto* child = this->activeModal()) child->setVisible(false);
        }
    }
    void parentViewportChanged() override
    {
        if (opened && centerOnOpen) center();
    }

    void handleEvent(Event event) override
    {
        if (!opened || !this->isInteractive()) return;
        if (this->activeModal())
        {
            Base::handleEvent(event);
            return;
        }
        if (event.type == EventType::TYPE)
        {
            if (event.character == '\x1b') { reject(); return; }
            if (acceptOnEnter && (event.character == '\r' || event.character == '\n'))
            {
                accept();
                return;
            }
        }
        const auto box = this->bounds();
        const int width = min(int(TitleHeight), box.width);
        const PixmapRect closeButton{panelCoordinate(int64_t(box.x) + box.width - width),
                                     box.y, width, contentRect().y};
        if ((event.type == EventType::CLICK || event.type == EventType::DOUBLE_CLICK) &&
            panelContains(closeButton, event.startX, event.startY) &&
            panelContains(closeButton, event.endX, event.endY))
        {
            reject();
            return;
        }
        Base::handleEvent(event);
    }

    void render(const PanelCanvas& canvas, int x, int y) override
    {
        Base::render(canvas, x, y);
        const auto box = panelTranslate(this->bounds(), x, y);
        const int height = contentRect().y;
        if (!height || !box.width) return;
        auto chrome = canvas;
        chrome.clip = panelIntersection(canvas.clip, box);
        const int button = min(int(TitleHeight), box.width);
        const int closeX = panelCoordinate(int64_t(box.x) + box.width - button);
        const int textHeight = max(1, min(height, theme.textHeight));
        const int pad = min(max(0, theme.padding), box.width - button);
        chrome.fill({box.x, box.y, box.width, height}, theme.surface);
        chrome.fill({closeX, box.y, button, height}, theme.border);
        chrome.text({box.x + pad, box.y + (height - textHeight) / 2,
                     box.width - button - pad, textHeight},
                    title, theme.font, theme.text, textHeight, theme.fontFilter);
        const int mark = min(7, min(button, height));
        for (int i = 0; i < mark; ++i)
        {
            const int px = closeX + (button - mark) / 2 + i;
            const int py = box.y + (height - mark) / 2;
            chrome.fill({px, py + i, 1, 1}, theme.text);
            chrome.fill({px, py + mark - 1 - i, 1, 1}, theme.text);
        }
    }
};
