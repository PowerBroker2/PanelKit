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
    // 1. Safety check to make sure the target component isn't null
    if (!child) return;

    // 2. Unlink the forward path from the previous component
    if (child->prevSibling != nullptr)
    {
        child->prevSibling->nextSibling = child->nextSibling;
    }
    else
    {
        // If it doesn't have a previous sibling, it was the absolute head
        head = child->nextSibling;
    }

    // 3. Unlink the backward path from the next component
    if (child->nextSibling != nullptr)
    {
        child->nextSibling->prevSibling = child->prevSibling;
    }

    // 4. Update current index and current pointer state trackers
    if (currentComp == child)
    {
        // Fall back gracefully to the next available component, or the previous one, or null
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

    // 5. Clear structural tracking data on the child so it doesn't leak dead pointers
    child->nextSibling = nullptr;
    child->prevSibling = nullptr;

    // 6. Reduce global tracker capacity size
    if (size > 0) size--;
}

void ComponentRegistry::updateOrder()
{
    // 1. If the list is empty or only has one item, no sorting is required
    if (head == nullptr || head->nextSibling == nullptr) return;

    // 2. Disconnect the entire existing chain from the registry management
    // Grab the first element to process, then wipe our structural track states
    BaseComponent* current = head;
    
    head        = nullptr;
    currentComp = nullptr;
    currentIdx  = 0;
    size        = 0; // registerComponent will increment this back up step-by-step

    // 3. Loop through the disconnected components and re-insert them 
    while (current != nullptr)
    {
        // CRITICAL: Save the next pointer *before* registerComponent overwrites it!
        BaseComponent* nextItem = current->nextSibling;

        // Strip structural pointers to prevent registerComponent from reading corrupt old links
        current->nextSibling = nullptr;
        current->prevSibling = nullptr;

        // Leverage your existing sorted insertion engine
        this->registerComponent(current);

        // Move to the next item in the legacy sequence
        current = nextItem;
    }
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
