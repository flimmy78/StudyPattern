#include "SystemInclude.h"
#include "Debug.h"

#include "TimerQueue/TimerQueue.h"  //TimerQueue
#include "Reactor/WfmoReactorNotify.h"

#include "Reactor/WfmoReactor.h"
using namespace std;

/**********************class WfmoReactorHandlerRepository**********************/
WfmoReactorHandlerRepository::WfmoReactorHandlerRepository()
{}

WfmoReactorHandlerRepository::~WfmoReactorHandlerRepository()
{}

shared_ptr<EventHandler> WfmoReactorHandlerRepository::Find(Handle handle)
{
    auto iter = repository.find(handle);
    if (iter == repository.end())
        return nullptr;

    return iter->second;
}

Handle* WfmoReactorHandlerRepository::GetEventHandles()
{
    return handles;
}

size_t WfmoReactorHandlerRepository::GetSize()
{
    return repository.size();
}

void WfmoReactorHandlerRepository::Insert(std::shared_ptr<EventHandler> handler)
{
    assert(repository.find(handler->GetEventHandle()) == repository.end());
    auto ret = repository.insert(make_pair(handler->GetEventHandle(), handler));
    assert(ret.second);

    handles[repository.size() - 1] = handler->GetEventHandle();
}

/**********************class WfmoReactor**********************/
WfmoReactor::WfmoReactor()
    : notifyHandler(nullptr), timerQueue(nullptr)
{
    if (!Open())
    {
        errstrm << "Open() failed." << endl;
    }
}

WfmoReactor::WfmoReactor(std::shared_ptr<WfmoReactorNotify> notifyHandler, 
                         std::shared_ptr<TimerQueue> timerQueue)
    : notifyHandler(notifyHandler), timerQueue(timerQueue)
{
    if (!Open())
    {
        errstrm << "Open() failed." << endl;
    }
}

WfmoReactor::~WfmoReactor()
{
}

bool WfmoReactor::IsActived()
{
    return true;
}

bool WfmoReactor::HandleEvents(Duration duration)
{
    return HandleEventsImpl(CalculateTimeout(duration));
}

bool WfmoReactor::Notify(std::shared_ptr<EventHandler> handler,
                               long mask)
{
    return notifyHandler->Notify(handler, mask);
}

bool WfmoReactor::Open()
{
    if (timerQueue == nullptr)
        timerQueue = make_shared<TimerQueue>();

    if (notifyHandler == nullptr)
        notifyHandler = make_shared<WfmoReactorNotify>();
    
    return notifyHandler->Open(this, timerQueue);
}

bool WfmoReactor::RegisterHandler(shared_ptr<EventHandler> handler)
{
    return RegisterHandlerImpl(handler);
}

bool WfmoReactor::ScheduleTimer(std::shared_ptr<EventHandler> handler,
        const void *arg,
        TimePoint timePoint,
        Duration  interval)
{
    return timerQueue->Schedule(handler, arg, timePoint, interval, nullptr);
}

/**********************class WfmoReactor**********************/
/* protected member function */
Duration WfmoReactor::CalculateTimeout(Duration maxWaitTime)
{
    Duration duration = timerQueue->CalculateTimeout();

    if (maxWaitTime > duration)
        return duration;

    return maxWaitTime;
}

bool WfmoReactor::DeregisterHandlerImpl(shared_ptr<EventHandler> handler, 
                                        long mask)
{
    long newMask = handler->GetMask() & (~mask);
    handler->SetMask(newMask);

    int result = ::WSAEventSelect((SOCKET)handler->GetIoHandle(),
        handler->GetEventHandle(), handler->GetMask());
    if (result != 0)
    {
        errstrm << "WSAEventSelect() failed, error code = " << WSAGetLastError() << endl;
        return false;
    }

    return true;
}

/* we don't return handler error code to my caller, the handler should take care it's own error 
*  status.
*/
void WfmoReactor::Dispatch (shared_ptr<EventHandler> handler)
{    
    if (handler->GetIoHandle() == InvalidHandleValue)
    {
        if (!UpCall(handler))
        {
            errstrm << "UpCall() failed. " << endl;
        }
        return;
    }

    WSANETWORKEVENTS events;
    if (::WSAEnumNetworkEvents((SOCKET)handler->GetIoHandle(), handler->GetEventHandle(),
                                &events) == SOCKET_ERROR)
    {
        errstrm << "::WSAEnumNetworkEvents() failed." << endl;
        return;
    }

    long problems = UpCall(events.lNetworkEvents, handler);
    if (problems != EventHandler::NullMask)
    {
        this->DeregisterHandlerImpl(handler, problems);
        errstrm << "UpCall() failed. " << endl;
        return;
    }
}

uint_t WfmoReactor::ExpireTimers()
{
    return this->timerQueue->Expire();
}

/* Reactor::RunReactorEventLoop() 
    -> implementation->HandleEvents()
    -> HandleEventsImpl(CalculateTimeout(duration))
  refert to ACE_WFMO_Reactor::event_handling
*/
bool WfmoReactor::HandleEventsImpl(Duration duration)
{
    if (!isActived)
    {
        return false;
    }   
    
    TimeCountDown countdown(duration);
    do
    {
        if (!this->WaitForMultipleEvents(duration, 0))
        {
            break;
        }

        ExpireTimers();    
        duration = countdown.GetRemainingTime();
    }while (duration != Duration::zero());

    return true;
}

bool WfmoReactor::RegisterHandlerImpl(shared_ptr<EventHandler> handler)
{
    if (handler->GetIoHandle() != InvalidHandleValue)
    {
        int result = ::WSAEventSelect((SOCKET)handler->GetIoHandle(),
            handler->GetEventHandle(), handler->GetMask());

        if (result == SOCKET_ERROR)
        {
            errstrm << "WSAEventSelect() failed, error code is " << WSAGetLastError() << endl;
            return false;
        }
    }
    repository.Insert(handler);

    return true;
}

bool WfmoReactor::UpCall(shared_ptr<EventHandler> handler)
{
    return handler->HandleSignal();
}

long WfmoReactor::UpCall(long events, shared_ptr<EventHandler> handler)
{
    long problems = EventHandler::NullMask;

    if ((events & FD_WRITE) != 0)
    {
        if (!handler->HandleOutput())
            problems = problems | FD_WRITE;
    }

    if ((events & FD_CONNECT) != 0)
    {
        if (!handler->HandleInput())
            problems = problems | FD_CONNECT;
    }

    if ((events & FD_OOB) != 0)
    {
        if (!handler->HandleException())
            problems = problems | FD_OOB;
    }

    if ((events & FD_READ) != 0)
    {
        if (!handler->HandleInput())
            problems = problems | FD_READ;
    }

    if ((events & FD_CLOSE) != 0)
    {
        if (!handler->HandleInput())
            problems = problems | FD_CLOSE;
    }
    
    if ((events & FD_ACCEPT) != 0)
    {
        if (!handler->HandleInput())
            problems = problems | FD_ACCEPT;
    }

    if ((events & FD_QOS) != 0)
    {
        if (!handler->HandleQos())
            problems = problems | FD_QOS;
    }
    
    if ((events & FD_GROUP_QOS) != 0)
    {
        if (!handler->HandleGroupQos())
            problems = problems | FD_GROUP_QOS;
    }

    return problems;
}

bool WfmoReactor::WaitForMultipleEvents(Duration duration, DWORD startIndex)
{    
    DWORD timeout = static_cast<DWORD>((duration == Duration::max()) ? INFINITE : duration.count() + 1);
    DWORD result;

    Handle *handles = repository.GetEventHandles();
    size_t handleNumber = repository.GetSize();

    if (handleNumber == 0 || handleNumber <=  startIndex)
    {
        return true;
    }

    result = ::WaitForMultipleObjectsEx(handleNumber - startIndex, 
        handles + startIndex, FALSE, timeout, TRUE); 
    switch (result)
    {
    case WAIT_FAILED:
        result = GetLastError();
        errstrm << "WaitForMultipleObjectsEx() failed, error code is " << result << endl;
        return false;

    case WAIT_TIMEOUT:      
    case WAIT_IO_COMPLETION:
        return true;

    default:
        break;
    }

    DWORD index;
    if (result <= WAIT_OBJECT_0 + repository.GetSize())
        index = result - WAIT_OBJECT_0 + startIndex;
    else
        index = result - WAIT_ABANDONED_0 + startIndex;
    
    shared_ptr<EventHandler> handler = repository.Find(handles[index]);
    Dispatch(handler);

    return WaitForMultipleEvents(Duration::zero(), index + 1);
}