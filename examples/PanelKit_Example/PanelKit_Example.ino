#include "PanelKit.h"

PanelButton<60, 60> btn;

void setup()
{
    btn.pixmap.draw_circle(50, 50, 5, 0, true,  ORANGE);
}

void loop()
{
    click(1, 60); 
    delay(500);
}