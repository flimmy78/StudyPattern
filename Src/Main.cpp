#include "SystemInclude.h"
#include "SystemError.h"

#include "Demo/ReactorDemo.h"

using namespace std;
void InitSocket();

int main(int argc, char *argv[])
{
    InitSocket();
    ReactorMain1(argc, argv);
    return 0;
}

void InitSocket()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != NO_ERROR) 
    {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.  */
        int errCode = WSAGetLastError();
        cerr << "Last error code " << errCode << endl;
        return;
    }
}
