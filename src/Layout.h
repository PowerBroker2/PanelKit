#pragma once
#include <Arduino.h>
#include "Events.h"

class BaseComponent; 

class LayoutManager
{
private:
    BaseComponent* head = nullptr;

public:
    LayoutManager();

    void registerChild(BaseComponent* child);
    void handleEvent(Event e);
    void activateAll();
    void deactivateAll();
};
