#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"
#include "Events.h"
#include "Component.h"

typedef void (*CallbackFunction)();

template <size_t WIDTH, size_t HEIGHT>
class PanelButton: public Component<WIDTH, HEIGHT>
{
protected:
    CallbackFunction _onClick;

    void handleEvent(Event e) override
    {
        if (e.type == EventType::CLICK)
            click();
    }

    void click()
    {
        if (_onClick != nullptr)
            _onClick();
    }

public:
    void onClick(CallbackFunction callback)
    {
        _onClick = callback;
    }
};