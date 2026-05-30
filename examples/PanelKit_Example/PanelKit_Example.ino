#include "TeensyTFT.h"
#include "PanelKit.h"

// TFT display
TeensyTFT display(10, 9, 255);

Panel<200, 200> panel;

PanelButton<60, 60> btn;
PanelButton<60, 60> btn2;

void setup()
{
    Serial.begin(115200);
    display.begin(80000000, 1);


    btn.setName("btn");
    btn.link(&panel);
    btn.pixmap.draw_circle(50, 50, 5, 0, true, ORANGE);
    btn.setLoc(20, 50);
    btn.activate();

    btn2.setName("btn2");
    btn2.link(&panel);
    btn2.pixmap.draw_circle(50, 50, 5, 0, true, RED);
    btn2.setLoc(0, 50);
    btn2.activate();


    display.clear();

    Coord loc = btn.getLoc();
    display.fillRegion(loc.x, loc.y, btn.pixmap.width(), btn.pixmap.height(), btn.pixmap.getPixels(), btn.pixmap.getMask());
    
    Coord loc2 = btn2.getLoc();
    display.fillRegion(loc2.x, loc2.y, btn2.pixmap.width(), btn2.pixmap.height(), btn2.pixmap.getPixels(), btn2.pixmap.getMask());
    
    display.swap();

    Serial.println(panel.currentComponent()->getName());
    panel.loadNextComponent();
    Serial.println(panel.currentComponent()->getName());
    panel.loadNextComponent();
    Serial.println(panel.currentComponent()->getName());
    Serial.println();

    btn.setOrder(0);
    btn2.setOrder(1);
    panel.updateOrder();
    panel.loadHeadComponent();

    Serial.println(panel.currentComponent()->getName());
    panel.loadNextComponent();
    Serial.println(panel.currentComponent()->getName());
    panel.loadNextComponent();
    Serial.println(panel.currentComponent()->getName());
    Serial.println();
}

void loop()
{
    press(0, 0);
    click(25, 60);
    delay(500);
}