#ifndef _Client_h_
#define _Client_h_

#include "SystemInclude.h"
#include "Task/Task.h"

class ReactorNotificationStrategy;
/**********************class Client**********************/
class Client: public Task
{
public:
    typedef Task MyBase;
    Client(Reactor *reactor);
    ~Client();
    
    virtual std::error_code HandleInput();
    virtual std::error_code HandleOutput();
    virtual std::error_code HandleTimeOut(TimePoint, const void *arg = 0);
        
    virtual std::error_code Open(void *args);

private:
    std::shared_ptr<ReactorNotificationStrategy> notifer;
};


#endif