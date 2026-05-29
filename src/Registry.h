#pragma once
#include <Arduino.h>
#include "Events.h"

class BaseComponent; 

class ComponentRegistry
{
private:
    BaseComponent* head        = nullptr;
    BaseComponent* currentComp = nullptr;
    int            currentIdx  = 0;
    int            size        = 0;

public:
    ComponentRegistry();

    void registerComponent(BaseComponent* child);
    void unregisterComponent(BaseComponent* child);
    void updateOrder();
    BaseComponent* currentComponent();
    BaseComponent* nextComponent();
    BaseComponent* prevComponent();
    BaseComponent* getComponentByIdx(int idx);
    BaseComponent* getComponentByName(char* name);
};
