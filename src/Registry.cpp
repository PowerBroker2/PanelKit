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

void ComponentRegistry::unregisterComponent(BaseComponent* child)
{
    if (!child)
        return;

    if (child->prevSibling != nullptr)
    {
        child->prevSibling->nextSibling = child->nextSibling;
    }
    else
    {
        head = child->nextSibling;
    }

    if (child->nextSibling != nullptr)
    {
        child->nextSibling->prevSibling = child->prevSibling;
    }

    if (currentComp == child)
    {
        if (child->nextSibling != nullptr)
        {
            currentComp = child->nextSibling;
            // index stays numerically identical because the list collapsed forward
        }
        else if (child->prevSibling != nullptr)
        {
            currentComp = child->prevSibling;
            currentIdx--;
        }
        else
        {
            currentComp = nullptr;
            currentIdx = 0;
        }
    }
    else
    {
        // Optional: If the removed component sat physically *before* our active component,
        // we must manually shift our tracked integer index back by 1 to maintain sync.
        // To do this strictly, you can traverse from head to find the actual index,
        // or just leave index management to an absolute recalculation function.
    }

    child->nextSibling = nullptr;
    child->prevSibling = nullptr;

    if (size > 0)
        size--;
}

void ComponentRegistry::updateOrder()
{
    if (head == nullptr || head->nextSibling == nullptr)
        return;

    BaseComponent* current = head;
    
    head        = nullptr;
    currentComp = nullptr;
    currentIdx  = 0;
    size        = 0;

    while (current != nullptr)
    {
        BaseComponent* nextItem = current->nextSibling;

        this->registerComponent(current);

        current = nextItem;
    }
}

BaseComponent* ComponentRegistry::currentComponent()
{
    return currentComp;
}

void ComponentRegistry::loadHeadComponent()
{
    currentComp = head;
    currentIdx = 0;
}

void ComponentRegistry::loadNextComponent()
{
    if (currentComp->nextSibling != nullptr)
    {
        currentComp = currentComp->nextSibling;
        currentIdx++;
    }
    else
    {
        currentComp = head;
        currentIdx  = 0;
    }
}

void ComponentRegistry::loadPrevComponent()
{
    if (currentComp->prevSibling != nullptr)
    {
        currentComp = currentComp->prevSibling;
        currentIdx--;
    }
    else
    {
        BaseComponent* tail = head;
        int tailIdx = 0;
        
        while (tail->nextSibling != nullptr)
        {
            tail = tail->nextSibling;
            tailIdx++;
        }
        
        currentComp = tail;
        currentIdx  = tailIdx;
    }
}

void ComponentRegistry::loadComponentByIdx(int idx)
{
    if (head == nullptr || idx < 0 || idx >= size)
        return;

    BaseComponent* current = head;
    int currentCount = 0;

    while (current != nullptr)
    {
        if (currentCount == idx)
        {
            currentComp = current;
            currentIdx  = currentCount;
            return;
        }
        current = current->nextSibling;
        currentCount++;
    }
}

void ComponentRegistry::loadComponentByName(char* name)
{
    if (head == nullptr || name == nullptr) return;

    BaseComponent* current = head;
    int currentCount = 0;

    while (current != nullptr)
    {
        const char* compName = current->getName();

        if (compName != nullptr)
        {
            if (strcmp(compName, name) == 0)
            {
                currentComp = current;
                currentIdx  = currentCount;
                return;
            }
        }
        
        current = current->nextSibling;
        currentCount++;
    }
}
