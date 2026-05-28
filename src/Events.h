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

Signal<Event> eventOccured;

void click(int x, int y)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::CLICK;
    e.character = '\0';
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = 0;

    eventOccured.fire(e);
}

void doubleClick(int x, int y)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::DOUBLE_CLICK;
    e.character = '\0';
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = 0;

    eventOccured.fire(e);
}

void scroll(int x, int y, float direction)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::SCROLL;
    e.character = '\0';
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = direction;

    eventOccured.fire(e);
}

void type(char c, int x = -1, int y = -1)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::TYPE;
    e.character = c;
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = 0;

    eventOccured.fire(e);
}

void press(int x, int y)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::PRESS;
    e.character = '\0';
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = 0;

    eventOccured.fire(e);
}

void release(int x, int y)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::RELEASE;
    e.character = '\0';
    e.startX    = x;
    e.startY    = y;
    e.endX      = x;
    e.endY      = y;
    e.direction = 0;

    eventOccured.fire(e);
}

void drag(int startX, int startY, int endX, int endY)
{
    Event e;

    e.timestamp = micros();
    e.type      = EventType::DRAG;
    e.character = '\0';
    e.startX    = startX;
    e.startY    = startY;
    e.endX      = endX;
    e.endY      = endY;
    e.direction = atan2(endY - startY, endX - startX) * 180.0 / M_PI;

    eventOccured.fire(e);
}