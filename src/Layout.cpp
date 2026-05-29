#include "Layout.h"
#include "Component.h"
#include "Events.h"

LayoutManager::LayoutManager() : head(nullptr) {}

void LayoutManager::registerChild(BaseComponent* child) 
{
    if (!child) return;

    if (head == nullptr || child->getOrder() <= head->getOrder()) 
    {
        child->nextSibling = head;
        head = child;
    }
    else
    {
        BaseComponent* current = head;
        while ((current->nextSibling != nullptr) && (current->nextSibling->getOrder() < child->getOrder())) 
        {
            current = current->nextSibling;
        }
        child->nextSibling   = current->nextSibling;
        current->nextSibling = child;
    }
}

void LayoutManager::handleEvent(Event e)
{
    BaseComponent* current = head;
    while (current != nullptr) 
    {
        current = current->nextSibling;
    }
}

void LayoutManager::activateAll() 
{
    BaseComponent* current = head;
    while (current != nullptr) 
    {
        current->activate();
        current = current->nextSibling;
    }
}

void LayoutManager::deactivateAll() 
{
    BaseComponent* current = head;
    while (current != nullptr) 
    {
        current->deactivate();
        current = current->nextSibling;
    }
}
