#include "SystemInclude.h"
#include "Debug.h"

#include "MessageQueue/MessageBlock.h"
#include "MessageQueue/NotificationStrategy.h"
#include "MessageQueue/MessageQueue.h"

using namespace std;

/**********************class MessageQueueBase**********************/
MessageQueueBase::MessageQueueBase()
{}

MessageQueueBase::~MessageQueueBase()
{}


/**********************class MessageQueueBase**********************/
MessageQueue::MessageQueue()
{
    Open(nullptr);
}

MessageQueue::MessageQueue(shared_ptr<NotificationStrategy> ns)
{
    Open(ns);
}

MessageQueue::~MessageQueue()
{
    Close();
}

void MessageQueue::Activate()
{
    lock_guard<mutex> lock(theMutex);
    this->state = Actived;
}

void MessageQueue::Close(void)
{
    lock_guard<mutex> lock(theMutex);
    DeactivateImpl();
    while(!msgQueue.empty())
    {
        msgQueue.pop();
    }
}

void MessageQueue::Deactivate(void)
{
    lock_guard<mutex> lock(theMutex);
    this->state = Deactivated;
}

bool MessageQueue::IsEmpty()
{
    lock_guard<mutex> lock(theMutex);
    return msgQueue.empty();
}

bool MessageQueue::IsFull()
{
    lock_guard<mutex> lock(theMutex);
    return (msgQueue.size() == MaxQueueSize);
}

size_t MessageQueue::GetSize()
{
    lock_guard<mutex> lock(theMutex);
    return msgQueue.size();
}

void MessageQueue::Open(shared_ptr<NotificationStrategy> ns)
{
    lock_guard<mutex> lock(theMutex);
    notificationStrategy = ns;
    this->state = Actived;
}

void MessageQueue::SetNotificationStrategy(shared_ptr<NotificationStrategy> ns)
{
    lock_guard<mutex> lock(theMutex);
    notificationStrategy = ns;
    this->state = Actived;
}

bool MessageQueue::PeekTop(shared_ptr<MessageBlock> &msg, Duration duration)
{
    unique_lock<mutex> lock(theMutex);
    assert(state != Deactivated);
    while (msgQueue.size() == 0)
    {
        if (theCv.wait_for(lock, duration) == cv_status::timeout)
        {
            lock.unlock();
            /* time out */
            errstrm << "error, queue is empty." << endl;
            return false;
        }
    }

    msg = msgQueue.top();
    theCv.notify_all();
    lock.unlock();
    
    if (notificationStrategy != nullptr)
        notificationStrategy->Notify();
    return true;
}

bool MessageQueue::Pop(shared_ptr<MessageBlock> &msg, Duration duration)
{
    unique_lock<mutex> lock(theMutex);
    assert(state != Deactivated);
    while (msgQueue.empty())
    {
        if (theCv.wait_for(lock, duration) == cv_status::timeout)
        {
            lock.unlock();
            return false;
        }
    }

    msg = msgQueue.top();
    msgQueue.pop();
    theCv.notify_all();
    lock.unlock();
    
    if (notificationStrategy != nullptr)
        notificationStrategy->Notify();
    return true;
}

bool MessageQueue::Push(shared_ptr<MessageBlock> msg, Duration duration)
{
    unique_lock<mutex> lock(theMutex);   
    assert(state != Deactivated); 
    while (msgQueue.size() == MaxQueueSize)
    {
        if (theCv.wait_for(lock, duration) == cv_status::timeout)
        {
            lock.unlock();
            /* time out */
            errstrm << "error, queue is full." << endl;
            return false;
        }
    }

    msgQueue.push(msg);
    theCv.notify_all();
    lock.unlock(); 

    if (notificationStrategy != nullptr)
        notificationStrategy->Notify();
    return true;
}

/**********************class MessageQueueBase**********************/
/* private member function */
void MessageQueue::DeactivateImpl(void)
{
}
