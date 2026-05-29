#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "Component.h"

template <size_t WIDTH, size_t HEIGHT>
class Panel : public Component<WIDTH, HEIGHT>
{
private:
    void handleEvent(Event e)
    {
        if (this->active)
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

            if (this->inBounds(e.startX, e.startY))
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
    Panel()
    {
        eventOccured.attach(MethodSlot<Panel<WIDTH, HEIGHT>, Event>(this, &Panel<WIDTH, HEIGHT>::handleEvent));
    };

};