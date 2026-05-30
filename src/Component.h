#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"
#include "Events.h"
#include "Utils.h"
#include "Registry.h"

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

    virtual void activate()   = 0;
    virtual void deactivate() = 0;

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
        int minX = ulhc.x; int minY = ulhc.y;
        int maxX = ulhc.x + dim.dx; int maxY = ulhc.y + dim.dy;
        return ((x <= maxX) && (x >= minX) && (y <= maxY) && (y >= minY));
    }
};

template <size_t WIDTH, size_t HEIGHT>
class Component : public BaseComponent
{
public:
    Component(ComponentRegistry* parentRegistry = nullptr) : BaseComponent(parentRegistry)
    {
        dim.dx = WIDTH;
        dim.dy = HEIGHT;
    }
    void activate() override   { active = true; }
    void deactivate() override { active = false; }

    Pixmap<WIDTH, HEIGHT> pixmap;
};