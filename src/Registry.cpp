#include "Registry.h"
#include "Component.h"

ComponentRegistry::~ComponentRegistry()
{
    while (head)
        unregisterComponent(head);
}

void ComponentRegistry::registerComponent(BaseComponent* child)
{
    if (!child || child->registry)
        return;
    for (auto* owner = ownerComponent(); owner;
         owner = owner->registry ? owner->registry->ownerComponent() : nullptr)
        if (owner == child)
            return;
    BaseComponent* previous = nullptr;
    auto* next = head;
    while (next && next->getOrder() <= child->getOrder())
    {
        previous = next;
        next = next->nextSibling;
    }
    child->registry = this;
    child->registered = true;
    child->prevSibling = previous;
    child->nextSibling = next;
    if (previous) previous->nextSibling = child;
    else head = child;
    if (next) next->prevSibling = child;
    else tail = child;
    currentComp = child;
    ++size;
    child->registryChanged();
    invalidateRegistry();
}

void ComponentRegistry::cancelInput()
{
    auto* target = capture;
    capture = focus = nullptr;
    if (target) target->cancelInteraction();
}

void ComponentRegistry::unregisterComponent(BaseComponent* child)
{
    if (!child || child->registry != this)
        return;
    const auto next = child->nextSibling;
    const auto previous = child->prevSibling;
    if (previous) previous->nextSibling = next;
    else head = next;
    if (next) next->prevSibling = previous;
    else tail = previous;
    if (currentComp == child) currentComp = next ? next : previous;
    if (capture == child) capture = nullptr;
    if (focus == child) focus = nullptr;
    if (popup == child) popup = nullptr;
    if (modal == child) modal = nullptr;
    child->registry = nullptr;
    child->registered = false;
    child->nextSibling = child->prevSibling = nullptr;
    --size;
    child->cancelInteraction();
    child->registryChanged();
    invalidateRegistry();
}

void ComponentRegistry::updateOrder()
{
    // Stable insertion sort of the intrusive list. Ownership and capture stay intact.
    BaseComponent* sorted = nullptr;
    for (auto* node = head; node;)
    {
        auto* next = node->nextSibling;
        auto** position = &sorted;
        BaseComponent* previous = nullptr;
        while (*position && (*position)->getOrder() <= node->getOrder())
        {
            previous = *position;
            position = &previous->nextSibling;
        }
        node->prevSibling = previous;
        node->nextSibling = *position;
        if (*position) (*position)->prevSibling = node;
        *position = node;
        node = next;
    }
    head = sorted;
    tail = head;
    while (tail && tail->nextSibling)
        tail = tail->nextSibling;
    invalidateRegistry();
}

void ComponentRegistry::raise(BaseComponent* child)
{
    if (!child || child->registry != this || child == tail) return;
    auto* previous = child->prevSibling;
    auto* next = child->nextSibling;
    if (previous) previous->nextSibling = next;
    else head = next;
    next->prevSibling = previous;
    child->order = tail->order;
    child->prevSibling = tail;
    child->nextSibling = nullptr;
    tail->nextSibling = child;
    tail = child;
    invalidateRegion(child->visualBounds());
}

BaseComponent* ComponentRegistry::hit(int x, int y) const
{
    if (popup && popup->isInteractive() && popup->hitTest(x, y))
        return popup;
    if (modal && modal->isInteractive())
        return modal->hitTest(x, y) ? modal : nullptr;
    for (auto* child = tail; child; child = child->prevSibling)
        if (child->isInteractive() && child->hitTest(x, y))
            return child;
    return nullptr;
}

void ComponentRegistry::setPopup(BaseComponent* child)
{
    if (child && child->registry != this) return;
    if (child == popup) return;
    auto* prior = popup;
    popup = nullptr;
    if (prior && child) prior->dismissPopup();
    popup = child;
}

void ComponentRegistry::setModal(BaseComponent* child)
{
    if ((child && child->registry != this) || child == modal) return;
    if (popup)
    {
        auto* prior = popup;
        popup = nullptr;
        prior->dismissPopup();
    }
    cancelInput();
    modal = child;
    invalidateRegistry();
}

void ComponentRegistry::dispatch(Event e)
{
    BaseComponent* target = nullptr;
    switch (e.type)
    {
        case EventType::PRESS:
            if (popup && !popup->hitTest(e.startX, e.startY))
            {
                auto* prior = popup;
                popup = nullptr;
                prior->dismissPopup();
            }
            cancelInput();
            capture = focus = hit(e.startX, e.startY);
            target = capture;
            break;
        case EventType::DRAG:
        case EventType::RELEASE:
            target = capture;
            if (e.type == EventType::RELEASE) capture = nullptr;
            break;
        case EventType::TYPE:
            target = focus;
            break;
        default:
            target = hit(e.startX, e.startY);
            break;
    }
    if (target && target->isInteractive())
        target->handleEvent(e);
}

void ComponentRegistry::renderChildren(const PanelCanvas& canvas, int x, int y)
{
    for (auto* child = head; child; child = child->nextSibling)
        if (child != popup && child != modal && child->isVisible())
        {
            const auto region = panelIntersection(canvas.clip, panelTranslate(child->visualBounds(), x, y));
            if (region.width && region.height)
                child->render(canvas, x, y);
        }
    if (modal && modal != popup && modal->isVisible())
        modal->render(canvas, x, y);
    if (popup && popup->isVisible())
        popup->render(canvas, x, y);
}

int ComponentRegistry::currentIndex()
{
    int index = 0;
    for (auto* item = head; item && item != currentComp; item = item->nextSibling)
        ++index;
    return currentComp ? index : 0;
}

void ComponentRegistry::loadComponentByIdx(int index)
{
    if (index < 0 || index >= size)
        return;
    auto* item = head;
    while (index-- > 0)
        item = item->nextSibling;
    currentComp = item;
}

void ComponentRegistry::loadComponentByName(const char* name)
{
    if (!name) return;
    for (auto* item = head; item; item = item->nextSibling)
        if (strcmp(item->getName(), name) == 0)
        {
            currentComp = item;
            return;
        }
}

void ComponentRegistry::loadNextComponent()
{
    currentComp = currentComp && currentComp->nextSibling ? currentComp->nextSibling : head;
}

void ComponentRegistry::loadPrevComponent()
{
    currentComp = currentComp && currentComp->prevSibling ? currentComp->prevSibling : tail;
}
