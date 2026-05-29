#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"
#include "Events.h"
#include "Utils.h"
#include "Layout.h"

class BaseComponent
{
protected:
    Coord     ulhc; 
    Dimension dim;
    
    BaseComponent* nextSibling  = nullptr; 
    LayoutManager* parentLayout = nullptr; 

    int order = 0; 
    bool active = false;

    friend class LayoutManager;

public:
    BaseComponent(LayoutManager* parent = nullptr)
    {
        if (parent != nullptr) 
        {
            this->parentLayout = parent;
            parent->registerChild(this); 
        }
    }

    virtual ~BaseComponent() {}
    virtual void activate()   = 0;
    virtual void deactivate() = 0;

    void setOrder(unsigned int orderVal) { order = (orderVal < 0) ? 0 : orderVal; }
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
    Component()
    {
        dim.dx = WIDTH;
        dim.dy = HEIGHT;
    }
    void activate() override   { active = true; }
    void deactivate() override { active = false; }

    Pixmap<WIDTH, HEIGHT> pixmap;
};