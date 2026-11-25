#pragma once
#include <Arduino.h>
#include "Events.h"
#include "Render.h"

class BaseComponent;
struct PanelCanvas;

class ComponentRegistry
{
protected:
    BaseComponent* head = nullptr;
    BaseComponent* tail = nullptr;
    BaseComponent* currentComp = nullptr;
    BaseComponent* capture = nullptr;
    BaseComponent* focus = nullptr;
    BaseComponent* popup = nullptr;
    BaseComponent* modal = nullptr;
    BaseComponent* modalReturnFocus = nullptr;
    int size = 0;

public:
    ComponentRegistry() = default;
    virtual ~ComponentRegistry();
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    void registerComponent(BaseComponent* child);
    void unregisterComponent(BaseComponent* child);
    void updateOrder();
    void raise(BaseComponent* child);
    virtual void invalidateRegistry() {}
    virtual void invalidateRegion(PixmapRect) { invalidateRegistry(); }
    void setPopup(BaseComponent* child);
    void dismissPopups();
    void setModal(BaseComponent* child);
    BaseComponent* activeModal() const;
    bool setInputFocus(BaseComponent* child);
    void clearModal(BaseComponent* child) { if (modal == child) setModal(nullptr); }
    virtual PixmapRect visibleArea() const { return {0, 0, INT_MAX, INT_MAX}; }
    virtual BaseComponent* ownerComponent() { return nullptr; }
    virtual void childVisibilityChanged() {}
    bool contains(const BaseComponent* child) const;
    void notifyViewportChanged();
    void cancelInput();
    void dispatch(Event event);
    void renderChildren(const PanelCanvas& canvas, int x, int y);
    BaseComponent* hit(int x, int y) const;

    int numComps() const { return size; }
    int currentIndex();
    BaseComponent* currentComponent() const { return currentComp; }
    BaseComponent* headComponent() const { return head; }
    void loadHeadComponent() { currentComp = head; }
    void loadNextComponent();
    void loadPrevComponent();
    void loadComponentByIdx(int idx);
    void loadComponentByName(const char* name);
};
