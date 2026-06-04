#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"
#include "Events.h"
#include "Utils.h"
#include "Registry.h"

typedef void (*CallbackFunction)();

const int MAX_NAME_SIZE = 25;

class BaseComponent
{
protected:
    char name[MAX_NAME_SIZE] = {'\0'};

    Coord     ulhc;
    Dimension dim;
    
    BaseComponent*     nextSibling  = nullptr;
    BaseComponent*     prevSibling  = nullptr;
    ComponentRegistry* registry     = nullptr;

    int order       = 0; 
    bool active     = false;
    bool registered = false;

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

    virtual void activate()   { active = true;  }
    virtual void deactivate() { active = false; }
            bool isActive()   { return active;  }

    void link(ComponentRegistry* parentRegistry)
    {
        if ((parentRegistry != nullptr) && (!registered))
        {
            this->registry = parentRegistry;
            parentRegistry->registerComponent(this); 
            registered = true;
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
        order = (orderVal < 0) ? 0 : orderVal;

        if ((registry != nullptr) && registered)
            registry->updateOrder();
    }

    int getOrder() const { return order; }
    void setLoc(int x, int y)  { ulhc.x = x; ulhc.y = y; }
    Coord getLoc()             { return ulhc; }
    
    bool inBounds(int x, int y)
    {
        int minX = ulhc.x;
        int minY = ulhc.y;
        int maxX = ulhc.x + dim.dx;
        int maxY = ulhc.y + dim.dy;

        return ((x <= maxX) && (x >= minX) && (y <= maxY) && (y >= minY));
    }

    virtual void handleEvent(Event e) {};
};

template <size_t WIDTH, size_t HEIGHT>
class Component : public BaseComponent
{
protected:
    CallbackFunction _onClick;
    CallbackFunction _onDoubleClick;
    CallbackFunction _onScroll;
    CallbackFunction _onType;
    CallbackFunction _onPress;
    CallbackFunction _onRelease;
    CallbackFunction _onDrag;

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