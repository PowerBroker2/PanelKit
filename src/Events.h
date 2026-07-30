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

// Helper to convert EventType enum values to human-readable strings
const char* eventTypeToString(EventType type);

class TouchProcessor
{
private:
    // Gesture tracking state
    bool          wasTouched;
    int           startX, startY;
    int           lastX, lastY;
    bool          isDragging;
    
    // Double-click tracking state
    bool          hasPendingClick;
    unsigned long lastClickTime;    // Timestamp in microseconds
    int           lastClickX, lastClickY;

    // Threshold configurations
    int           dragThresholdPixels;
    int           dragThresholdSq;  // Pre-calculated squared radius for fast distance checks
    unsigned long doubleClickTimeUs; // Timeout in microseconds

    // Emission configuration
    bool          autoEmit;

    // Internal fixed buffer for frame events (no dynamic allocation)
    static constexpr size_t MAX_BUFFER_SIZE = 4;
    Event  eventBuffer[MAX_BUFFER_SIZE];
    size_t eventCount;

    // Internal helper to construct Event objects
    Event createEvent(EventType type, int sX, int sY, int eX, int eY, float dir = 0.0f) const
    {
        Event e;
        e.timestamp = micros();
        e.type      = type;
        e.character = '\0';
        e.startX    = sX;
        e.startY    = sY;
        e.endX      = eX;
        e.endY      = eY;
        e.direction = dir;
        return e;
    }

    // Add event to buffer safely
    void queueEvent(const Event& e)
    {
        if (eventCount < MAX_BUFFER_SIZE)
        {
            eventBuffer[eventCount++] = e;
        }
    }

public:
    TouchProcessor(bool autoEmitEvents = true, int dragThreshold = 10, unsigned long doubleClickMs = 300)
        : wasTouched(false),
          startX(0), startY(0),
          lastX(0), lastY(0),
          isDragging(false),
          hasPendingClick(false),
          lastClickTime(0),
          lastClickX(0), lastClickY(0),
          dragThresholdPixels(dragThreshold),
          dragThresholdSq(dragThreshold * dragThreshold),
          doubleClickTimeUs(doubleClickMs * 1000UL),
          autoEmit(autoEmitEvents),
          eventCount(0)
    {}

    // --- Configuration Setters/Getters ---
    void setAutoEmit(bool enable) { autoEmit = enable; }
    bool getAutoEmit() const { return autoEmit; }
    
    void setDragThreshold(int pixels) 
    { 
        dragThresholdPixels = pixels; 
        dragThresholdSq     = pixels * pixels; 
    }
    int getDragThreshold() const { return dragThresholdPixels; }

    void setDoubleClickTime(unsigned long ms) { doubleClickTimeUs = ms * 1000UL; }
    unsigned long getDoubleClickTime() const { return doubleClickTimeUs / 1000UL; }

    // --- Buffer Access Methods ---
    size_t getEventCount() const { return eventCount; }
    bool hasEvents() const { return eventCount > 0; }

    const Event* getEvent(size_t index) const
    {
        if (index < eventCount) return &eventBuffer[index];
        return nullptr;
    }

    // Pop the oldest event out of the buffer
    bool popEvent(Event& outEvent)
    {
        if (eventCount == 0) return false;
        outEvent = eventBuffer[0];

        for (size_t i = 1; i < eventCount; ++i)
        {
            eventBuffer[i - 1] = eventBuffer[i];
        }
        eventCount--;
        return true;
    }

    // Manually emit all queued events to global signal and clear buffer
    void emitPendingEvents()
    {
        for (size_t i = 0; i < eventCount; ++i)
        {
            getEventSignal().fire(eventBuffer[i]);
        }
        eventCount = 0;
    }

    // Clear buffer without emitting
    void clearEvents()
    {
        eventCount = 0;
    }

    // --- Main Gesture State Machine ---
    void process(bool isTouched, int x, int y)
    {
        // Reset frame buffer
        eventCount = 0;

        const unsigned long now = micros();

        // 1. TOUCH STARTED (UNTOUCHED -> TOUCHED)
        if (isTouched && !wasTouched)
        {
            wasTouched = true;
            startX     = x;
            startY     = y;
            lastX      = x;
            lastY      = y;
            isDragging = false;

            queueEvent(createEvent(EventType::PRESS, x, y, x, y));
        }
        // 2. TOUCH CONTINUING (Evaluate Drag)
        else if (isTouched && wasTouched)
        {
            // Evaluate Radial Touch Slop (squared distance check)
            if (!isDragging)
            {
                const int dx = x - startX;
                const int dy = y - startY;
                
                if ((dx * dx + dy * dy) > dragThresholdSq)
                {
                    isDragging      = true;
                    hasPendingClick = false; // Invalidate double-click tracking on drag start
                }
            }

            // Fire DRAG events only when coordinate changes occur
            if (isDragging)
            {
                if (x != lastX || y != lastY)
                {
                    const float dir = atan2f(static_cast<float>(y - lastY), 
                                             static_cast<float>(x - lastX)) * (180.0f / M_PI);
                    
                    queueEvent(createEvent(EventType::DRAG, lastX, lastY, x, y, dir));
                    lastX = x;
                    lastY = y;
                }
            }
        }
        // 3. TOUCH RELEASED (TOUCHED -> UNTOUCHED)
        else if (!isTouched && wasTouched)
        {
            wasTouched = false;

            queueEvent(createEvent(EventType::RELEASE, lastX, lastY, lastX, lastY));

            // Evaluate Click vs Double Click
            if (!isDragging)
            {
                const int clickDistX  = lastX - lastClickX;
                const int clickDistY  = lastY - lastClickY;
                const int clickDistSq = (clickDistX * clickDistX) + (clickDistY * clickDistY);

                if (hasPendingClick && 
                    (now - lastClickTime <= doubleClickTimeUs) && 
                    (clickDistSq <= dragThresholdSq))
                {
                    queueEvent(createEvent(EventType::DOUBLE_CLICK, lastX, lastY, lastX, lastY));
                    hasPendingClick = false; // Prevent triple-clicks from re-triggering
                }
                else
                {
                    queueEvent(createEvent(EventType::CLICK, lastX, lastY, lastX, lastY));
                    hasPendingClick = true;
                    lastClickTime   = now;
                    lastClickX      = lastX;
                    lastClickY      = lastY;
                }
            }
        }

        // Emit at the end of process() if autoEmit is enabled
        if (autoEmit)
        {
            emitPendingEvents();
        }
    }
};
