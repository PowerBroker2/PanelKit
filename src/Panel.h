#pragma once
#include <Arduino.h>
#include <Callback.h>
#include "Component.h"

template <size_t WIDTH, size_t HEIGHT>
class Panel : public Component<WIDTH, HEIGHT>, public ComponentRegistry
{
protected:
    void handleEvent(Event e) override
    {
        if (!this->active)
        {
            Serial.println("[Panel] Inactive - Event ignored.");
            return;
        }

        // --- Event Summary Header ---
        Serial.println("==================== [ PANEL EVENT ] ====================");
        Serial.print("Type:      "); Serial.println(eventTypeToString(e.type));
        Serial.print("Timestamp: "); Serial.print(e.timestamp); Serial.println(" us");
        Serial.print("Start Pos: ("); Serial.print(e.startX); Serial.print(", "); Serial.print(e.startY); Serial.println(")");

        // Print extra details only if applicable to event type
        if (e.type == EventType::DRAG)
        {
            Serial.print("End Pos:   ("); Serial.print(e.endX); Serial.print(", "); Serial.print(e.endY); Serial.println(")");
            Serial.print("Direction: "); Serial.print(e.direction); Serial.println(" deg");
        }
        else if (e.type == EventType::TYPE)
        {
            Serial.print("Character: '"); Serial.print(e.character); Serial.println("'");
        }

        // --- Bounds & Routing Logic ---
        if (this->inBounds(e.startX, e.startY))
        {
            Serial.println("Panel:     IN BOUNDS -> Dispatching to components...");

            loadHeadComponent();
            auto comp = currentComponent();

            while (comp != nullptr)
            {
                Serial.print("  |-- Child [");
                Serial.print(comp->getName() ? comp->getName() : "Unnamed");
                Serial.print("]: ");

                if (comp->inBounds(e.startX, e.startY))
                {
                    Serial.println("IN BOUNDS -> Forwarding event");
                    comp->handleEvent(e);
                }
                else
                {
                    Serial.println("OUT OF BOUNDS");
                }

                loadNextComponent();
                comp = currentComponent();

                if (comp == headComponent())
                    break;
            }
        }
        else
        {
            Serial.println("Panel:     OUT OF BOUNDS");
        }

        Serial.println("=========================================================\n");
    };

public:
    Panel(ComponentRegistry* parentRegistry = nullptr) : Component<WIDTH, HEIGHT>(parentRegistry), ComponentRegistry()
    {
        getEventSignal().attach(MethodSlot<Panel<WIDTH, HEIGHT>, Event>(this, &Panel<WIDTH, HEIGHT>::handleEvent));
    };
};
