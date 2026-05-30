#pragma once
#include <Arduino.h>
#include <Callback.h>

enum class EventType { CLICK,
                       DOUBLE_CLICK,
                       SCROLL,
                       TYPE,
                       PRESS,
                       RELEASE,
                       DRAG };

struct Event
{
    unsigned long timestamp;
    EventType     type;
    char          character;
    int           startX;
    int           startY;
    int           endX;
    int           endY;
    float         direction;
};

// Declare that the signal exists globally
// extern Signal<Event> eventOccured;
Signal<Event>& getEventSignal();

// Declare function prototypes (Note: Default parameters must remain in the header)
void click(int x, int y);
void doubleClick(int x, int y);
void scroll(int x, int y, float direction);
void type(char c, int x = -1, int y = -1);
void press(int x, int y);
void release(int x, int y);
void drag(int startX, int startY, int endX, int endY);