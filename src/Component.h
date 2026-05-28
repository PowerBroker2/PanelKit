#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "Events.h"
#include "Utils.h"

template <size_t WIDTH, size_t HEIGHT>
class Component
{
private:
    Coord ulhc;

    int order = 0; // 0 is top, 1 is a layer below, etc.

    bool active = false;

    void handleEvent(Event e)
    {
        if (active)
        {
            Serial.println("Active");
            Serial.println("Event");
            Serial.println(e.timestamp);
            Serial.println((int)e.type);
            Serial.println(e.character);
            Serial.println(e.startX);
            Serial.println(e.startY);
            Serial.println(e.endX);
            Serial.println(e.endY);
            Serial.println(e.direction);

            if (inBounds(e.startX, e.startY))
            {
                Serial.println("In Bounds");
            }
            else
            {
                Serial.println("Out of Bounds");
            }
        }
        else
        {
            Serial.println("Inactive");
        }

        Serial.println();
    };

public:
    Component()
    {
        eventOccured.attach(MethodSlot<Component, Event>(this, &Component::handleEvent));
    }

    void setLoc(int x, int y)
    {
        ulhc.x = x;
        ulhc.y = y;
    }

    Coord getLoc(int x, int y)
    {
        return ulhc;
    }

    void nudge(int dx, int dy)
    {
        ulhc.x += dx;
        ulhc.y += dy;
    }

    bool inBounds(int x, int y)
    {
        // Account for pixmap's offset on screen
        return pixmap.inBounds(x - ulhc.x, y - ulhc.y);
    }

    void activate()
    {
        active = true;

        // Do something with pixmap?
    };

    void deactivate()
    {
        active = false;

        // Do something with pixmap?
    };

    void setOrder(unsigned int orderVal)
    {
        if (orderVal < 0)
        {
            order = 0;
        }
        else
        {
            order = orderVal;
        }
    };

    Pixmap<WIDTH, HEIGHT> pixmap;
};