#include <windows.h>
#include <tchar.h>
#include <fstream>
#include <iostream>

// Service name
#define SERVICE_NAME _T("TimeChangeService")

SERVICE_STATUS g_ServiceStatus = {};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = NULL;

void LogEvent(const std::wstring& message) {
    std::wofstream logFile("TimeChangeService.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

DWORD WINAPI ServiceCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
    if (dwControl == SERVICE_CONTROL_STOP) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
    }
    else if (dwControl == SERVICE_CONTROL_TIMECHANGE && lpEventData) {
        SERVICE_TIMECHANGE_INFO* timeChangeInfo = (SERVICE_TIMECHANGE_INFO*)lpEventData;

        LARGE_INTEGER liOldTime = timeChangeInfo->liOldTime;
        LARGE_INTEGER liNewTime = timeChangeInfo->liNewTime;

        FILETIME ftOldTime, ftNewTime;
        ftOldTime.dwLowDateTime = liOldTime.LowPart;
        ftOldTime.dwHighDateTime = liOldTime.HighPart;

        ftNewTime.dwLowDateTime = liNewTime.LowPart;
        ftNewTime.dwHighDateTime = liNewTime.HighPart;

        SYSTEMTIME oldTime;
        SYSTEMTIME newTime;

        FileTimeToSystemTime(&ftOldTime, &oldTime);
        FileTimeToSystemTime(&ftNewTime, &newTime);

        wchar_t buffer[256];
        swprintf_s(buffer, sizeof(buffer) / sizeof(wchar_t),
            L"Time change detected: Old Time: %02d/%02d/%04d %02d:%02d:%02d -> New Time: %02d/%02d/%04d %02d:%02d:%02d\n",
            oldTime.wMonth, oldTime.wDay, oldTime.wYear, oldTime.wHour, oldTime.wMinute, oldTime.wSecond,
            newTime.wMonth, newTime.wDay, newTime.wYear, newTime.wHour, newTime.wMinute, newTime.wSecond);

        LogEvent(buffer);
    }

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    return NO_ERROR;
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceCtrlHandler, NULL);
    if (!g_StatusHandle) {
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_TIMECHANGE;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Create the stop event
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Wait for the stop event
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int _tmain(int argc, TCHAR* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        std::wofstream logFile("TimeChangeService.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << L"Failed to start service control dispatcher." << std::endl;
        }
    }

    return 0;
}
