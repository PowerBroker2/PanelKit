#include "Registry.h"
#include "Component.h"
#include "Events.h"

ComponentRegistry::ComponentRegistry() : head(nullptr) {}

void ComponentRegistry::registerComponent(BaseComponent* child) 
{
    if (!child) return;

    int tempIdx = 0;

    if (head == nullptr) 
    {
        child->nextSibling = nullptr;
        child->prevSibling = nullptr;

        head = child;
    }
    else if (child->getOrder() <= head->getOrder())
    {
        child->nextSibling = head;
        child->prevSibling = nullptr;

        head->prevSibling = child;

        head = child;
    }
    else
    {
        tempIdx++; // Add one for head right off the bat

        BaseComponent* current = head;

        while (true)
        {
            if (current->nextSibling == nullptr)
                break;

            if (child->getOrder() < current->nextSibling->getOrder())
                break;
            
            current = current->nextSibling;
            tempIdx++;
        }

        BaseComponent* prev = current;
        BaseComponent* next = current->nextSibling;

        child->nextSibling   = next;
        child->prevSibling   = prev;

        prev->nextSibling = child;

        if (next != nullptr)
            next->prevSibling = child;
    }

    currentIdx  = tempIdx;
    currentComp = child;
    size++;
}

void ComponentRegistry::updateOrder()
{
    // BaseComponent* component = head;

    // TODO
}

BaseComponent* ComponentRegistry::currentComponent()
{
    return currentComp;
}

BaseComponent* ComponentRegistry::nextComponent()
{
    if (currentComp->nextSibling != nullptr)
    {
        currentComp = currentComp->nextSibling;
        currentIdx++;

        return currentComp;
    }

    return nullptr;
}

BaseComponent* ComponentRegistry::prevComponent()
{
    BaseComponent* component  = nullptr;

    return component;
}

BaseComponent* ComponentRegistry::getComponentByIdx(int idx)
{
    BaseComponent* component  = nullptr;

    return component;
}

BaseComponent* ComponentRegistry::getComponentByName(char* name)
{
    BaseComponent* component  = nullptr;

    return component;
}
