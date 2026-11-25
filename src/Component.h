#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"
#include "Events.h"
#include "Utils.h"
#include "Registry.h"
#include "Render.h"

typedef void (*CallbackFunction)();

const int MAX_NAME_SIZE = 25;

class BaseComponent
{
protected:
    char name[MAX_NAME_SIZE] = {'\0'};

    Coord     ulhc{};
    Dimension dim{};
    
    BaseComponent*     nextSibling  = nullptr;
    BaseComponent*     prevSibling  = nullptr;
    ComponentRegistry* registry     = nullptr;

    int order       = 0; 
    bool active     = false;
    bool registered = false;
    bool visible    = true;
    bool renderDirty = true;

    friend class ComponentRegistry;

public:
    BaseComponent(ComponentRegistry* parentRegistry = nullptr)
    {
        link(parentRegistry);
    }

    virtual ~BaseComponent()
    {
        unlink();
    }
    BaseComponent(const BaseComponent&) = delete;
    BaseComponent& operator=(const BaseComponent&) = delete;

    virtual void activate()   { active = true; invalidate(); }
    virtual void deactivate() { active = false; invalidate(); }
    bool isActive() const { return active; }
    virtual bool isInteractive() const { return active && visible; }
    bool isVisible() const { return visible; }
    virtual void setVisible(bool value)
    {
        if (visible == value) return;
        visible = value;
        if (!visible) cancelInteraction();
        invalidate();
    }
    virtual void invalidate()
    {
        renderDirty = true;
        if (registry) registry->invalidateRegion(visualBounds());
    }

    void link(ComponentRegistry* parentRegistry)
    {
        if ((parentRegistry != nullptr) && (!registered))
        {
            parentRegistry->registerComponent(this); 
        }
    }

    void unlink()
    {
        if (registry != nullptr)
        {
            registry->unregisterComponent(this);
            registry   = nullptr;
            registered = false;
        }
    }

    void setName(const char* newName) 
    {
        if (newName == nullptr)
            return;

        strlcpy(name, newName, MAX_NAME_SIZE);
    }

    char* getName() 
    {
        return name;
    }

    void setOrder(unsigned int orderVal)
    {
        order = orderVal > unsigned(INT_MAX) ? INT_MAX : int(orderVal);

        if ((registry != nullptr) && registered)
            registry->updateOrder();
    }

    int getOrder() const { return order; }
    void setLoc(int x, int y)
    {
        if (x != ulhc.x || y != ulhc.y)
        {
            invalidate();
            ulhc = {x, y};
            invalidate();
        }
    }
    virtual void setSize(int width, int height)
    {
        const Dimension next{max(0, width), max(0, height)};
        if (next.dx == dim.dx && next.dy == dim.dy) return;
        invalidate();
        dim = next;
        invalidate();
    }
    Coord getLoc() const { return ulhc; }
    Dimension getSize() const { return dim; }
    PixmapRect bounds() const { return {ulhc.x, ulhc.y, dim.dx, dim.dy}; }
    virtual PixmapRect visualBounds() const { return bounds(); }
    virtual bool hitTest(int x, int y) const { return inBounds(x, y); }
    
    bool inBounds(int x, int y) const
    {
        return x >= ulhc.x && y >= ulhc.y &&
               int64_t(x) - ulhc.x < dim.dx && int64_t(y) - ulhc.y < dim.dy;
    }

    virtual void handleEvent(Event) {}
    virtual void render(const PanelCanvas&, int, int) {}
    virtual void cancelInteraction() {}
    virtual void registryChanged() {}
    virtual void dismissPopup() {}
};

template <size_t WIDTH, size_t HEIGHT>
class Component : public BaseComponent
{
protected:
    CallbackFunction _onClick = nullptr;
    CallbackFunction _onDoubleClick = nullptr;
    CallbackFunction _onScroll = nullptr;
    CallbackFunction _onType = nullptr;
    CallbackFunction _onPress = nullptr;
    CallbackFunction _onRelease = nullptr;
    CallbackFunction _onDrag = nullptr;

    void handleEvent(Event e) override
    {
        switch (e.type)
        {
            case EventType::CLICK:
            {
                click();
                break;
            }

            case EventType::DOUBLE_CLICK:
            {
                doubleClick();
                break;
            }

            case EventType::SCROLL:
            {
                scroll();
                break;
            }

            case EventType::TYPE:
            {
                type();
                break;
            }

            case EventType::PRESS:
            {
                press();
                break;
            }

            case EventType::RELEASE:
            {
                release();
                break;
            }

            case EventType::DRAG:
            {
                drag();
                break;
            }

            default:
            {
                // Do nothing
                break;
            }
        }
    }

    virtual void click()
    {
        if (_onClick != nullptr)
            _onClick();
    }

    virtual void doubleClick()
    {
        if (_onDoubleClick != nullptr)
            _onDoubleClick();
    }

    virtual void scroll()
    {
        if (_onScroll != nullptr)
            _onScroll();
    }

    virtual void type()
    {
        if (_onType != nullptr)
            _onType();
    }

    virtual void press()
    {
        if (_onPress != nullptr)
            _onPress();
    }

    virtual void release()
    {
        if (_onRelease != nullptr)
            _onRelease();
    }

    virtual void drag()
    {
        if (_onDrag != nullptr)
            _onDrag();
    }

public:
    Component(ComponentRegistry* parentRegistry = nullptr) : BaseComponent(parentRegistry)
    {
        dim.dx = WIDTH;
        dim.dy = HEIGHT;
    }

    Pixmap<WIDTH, HEIGHT> pixmap;
    void setSize(int w, int h) override
    {
        BaseComponent::setSize(min(w, int(WIDTH)), min(h, int(HEIGHT)));
    }
    void render(const PanelCanvas& canvas, int x, int y) override
    {
        auto viewport = canvas;
        viewport.clip = panelIntersection(canvas.clip, panelTranslate(bounds(), x, y));
        viewport.blit(panelTranslate({ulhc.x, ulhc.y, int(WIDTH), int(HEIGHT)}, x, y),
                      pixmap.getPixels(), pixmap.getMask());
        renderDirty = false;
    }
    
    void onClick(CallbackFunction callback)
    {
        _onClick = callback;
    }

    void onDoubleClick(CallbackFunction callback)
    {
        _onDoubleClick = callback;
    }
    
    void onScroll(CallbackFunction callback)
    {
        _onScroll = callback;
    }
    
    void onType(CallbackFunction callback)
    {
        _onType = callback;
    }
    
    void onPress(CallbackFunction callback)
    {
        _onPress = callback;
    }
    
    void onRelease(CallbackFunction callback)
    {
        _onRelease = callback;
    }
    
    void onDrag(CallbackFunction callback)
    {
        _onDrag = callback;
    }
};
