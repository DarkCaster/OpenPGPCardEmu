#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <signal.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include "standalone_config.h"
#include "main_loop.h"

BOOL WINAPI ConsoleHandler(DWORD dwCtrlType);

static volatile boolean hangup=false;

int main()
{
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE) == FALSE)
    {
        printf("Unable to install handler!\n");
        return -1;
    }
    printf("Starting main loop\n");
    fflush(stdout);
    while(!hangup)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        loop();
    }
    printf("Exit\n");
    return 0;
}

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    switch(CEvent)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        hangup=true;
        printf("Hangup...\n");
        break;
    default:
        break;
    }
    return TRUE;
}
