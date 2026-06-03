#include "TeensyTFT.h"
#include "PanelKit.h"

// TFT display
TeensyTFT display(10, 9, 255);

Panel<200, 200> panel;

PanelButton<60, 60> btn;
PanelButton<60, 60> btn2;

void btnCallback()
{
    Serial.println("btn --- hi");
}

void btn2Callback()
{
    Serial.println("btn2 --- hi");
}

void setup()
{
    Serial.begin(115200);
    display.begin();


    btn.setName("btn");
    btn.link(&panel);
    btn.pixmap.draw_circle(50, 50, 5, 0, true, ORANGE);
    btn.setLoc(20, 50);
    btn.onClick(btnCallback);
    btn.activate();

    btn2.setName("btn2");
    btn2.link(&panel);
    btn2.pixmap.draw_circle(50, 50, 5, 0, true, RED);
    btn2.setLoc(0, 50);
    btn2.onClick(btn2Callback);
    btn2.activate();

    panel.setLoc(25, 60);
    panel.activate();


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

    delay(2000);
}

void loop()
{
    if (display.touched())
    {
        click(display.touchX(), display.touchY());
    }
}