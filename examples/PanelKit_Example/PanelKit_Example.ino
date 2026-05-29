#include "TeensyTFT.h"
#include "PanelKit.h"

// TFT display
TeensyTFT display(10, 9, 255);

PanelButton<60, 60> btn;

void setup()
{
    Serial.begin(115200);
    display.begin(80000000, 1);

    btn.pixmap.draw_circle(50, 50, 5, 0, true,  ORANGE);
    btn.setLoc(20, 50);
    btn.activate();

    display.clear();
    Coord loc = btn.getLoc();
    display.fillRegion(loc.x, loc.y, btn.pixmap.width(), btn.pixmap.height(), btn.pixmap.getPixels(), btn.pixmap.getMask());
    display.swap();
}

void loop()
{
    press(0, 0);
    click(25, 60);
    delay(500);
}