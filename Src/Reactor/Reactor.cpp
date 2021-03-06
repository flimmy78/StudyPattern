#include "SystemInclude.h"

#include "Event/EventHandler.h" //EventHandler
#include "Reactor/ReactorImpl.h" //ReactorImpl
#include "Reactor/WfmoReactor.h"
#include "Reactor/Reactor.h"
using namespace std;

/**********************class Reactor**********************/
Reactor::Reactor()
{
#ifdef _WIN32
    implementation = make_shared<WfmoReactor>();
#endif
}

Reactor::~Reactor()
{
}

bool Reactor::Notify(std::shared_ptr<EventHandler> handler, long mask)
{
    if (handler != nullptr && handler->GetReactor() == nullptr)
        handler->SetReactor(this);

    return implementation->Notify(handler, mask);
}

bool Reactor::RegisterHandler(shared_ptr<EventHandler> handler)
{
    error_code errCode;
    Reactor *reactor = handler->GetReactor();
    handler->SetReactor(this);

    if (!implementation->RegisterHandler(handler))
    {
        handler->SetReactor(reactor);
        return false;
    }

    return true;
}

bool Reactor::RunEventLoop()
{
    return RunReactorEventLoop(nullptr);
}

bool Reactor::RunReactorEventLoop(ReactorEventHook hook)
{
    if (!implementation->IsActived())
    {
        return false;
    }

    while (true)
    {
        if (!implementation->HandleEvents(Duration::max()))
        { 
            break; 
        }

        if (hook != nullptr)
        {
            hook(*this);
        }
    }

    return true;
}

bool Reactor::ScheduleTimer(std::shared_ptr<EventHandler> handler,
        const void *arg,
        TimePoint timePoint,
        Duration  interval)
{
    // Remember the old reactor.
    Reactor *oldReactor = handler->GetReactor();
    handler->SetReactor(this);

    if (!this->implementation->ScheduleTimer(handler, arg, timePoint, interval))
    {
        // Reset the old reactor in case of failures.
        handler->SetReactor(oldReactor);
        return false;
    }

    return true;
}