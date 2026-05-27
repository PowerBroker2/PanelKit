#include <Arduino.h>
#include <Callback.h>
#include "PixelPlanner.h"

/* Detail even types */

template <size_t WIDTH, size_t HEIGHT>
class PanelButton
{
private:
    void handleEvent();
    void setEventCallback();

public:
    Pixmap<WIDTH, HEIGHT> pixmap;

    void setVisible(bool visibility);
    void setOrder(unsigned int order);
};