#ifndef WIN32_HUB_EVENTS_H
#define WIN32_HUB_EVENTS_H

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    // HUBEVENT_API
    //  - define as empty if you are compiling 'win32-hub-events.cpp' into your program, instead of using the DLL

#ifndef HUBEVENT_API
#define HUBEVENT_API __declspec(dllimport)
#endif

    // Producer APIs

    struct HubEvent;
    typedef HubEvent * HHEVENT;

    // CreateHubEvent
    //  - creates server side of Hub Event object
    //  - see CreateEvent for the parameters
    //     - lpName cannot be NULL because HHEVENT cannot be consumed (waited on) or duplicated
    //  - returns HHEVENT handle that can be used only in the rest of the API
    //     - it's not a regular kernel handle
    //     - returns NULL on error, call GetLastError () for more information
    //
    HUBEVENT_API HHEVENT WINAPI CreateHubEventW (_In_opt_ LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, _In_ LPCWSTR lpName);
    HUBEVENT_API HHEVENT WINAPI CreateHubEventA (_In_opt_ LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, _In_ LPCSTR lpName);

    // DestroyUmbrellaEvent
    //  - stops the server side of Hub Event and frees all resources
    //
    HUBEVENT_API BOOL WINAPI DestroyHubEvent (_In_ HHEVENT hHubEvent);

    // SetHubEvent
    //  - sets the event for all consumers
    //
    HUBEVENT_API BOOL WINAPI SetHubEvent (_In_ HHEVENT hHubEvent);

    // ResetHubEvent
    //  - resets the event for all consumers
    //
    HUBEVENT_API BOOL WINAPI ResetHubEvent (_In_ HHEVENT hHubEvent);

    // PulseHubEvent
    //  - pulses the event for all consumers
    //  - provided only for completeness, unreliable just as the bad old PulseEvent
    //
    HUBEVENT_API BOOL WINAPI PulseHubEvent (_In_ HHEVENT hHubEvent);

    // HUB_EVENT_INFO
    //  - various information and statistics collected internally by the Hub Event
    //
    typedef struct _HUB_EVENT_INFO {
        SIZE_T  nConsumersCount; // number of consumers currently consuming events
        SIZE_T  nConsumersTotal; // number of total consumer 'open's
        BOOL    bManualReset;
        BOOL    bInitialState;
    } HUB_EVENT_INFO, * PHUB_EVENT_INFO;

    // GetHubEventInfo
    //  - retrieves HUB_EVENT_INFO (or partial, depending on cbSize)
    //     - unknown fields (i.e. called by old API) are set to zero
    //  - hHubEvent MUST be valid HHEVENT or this function will crash
    //
    HUBEVENT_API BOOL WINAPI GetHubEventInfo (_In_ HHEVENT hHubEvent,
                                              _Out_writes_bytes_ (cbSize) PHUB_EVENT_INFO lpHubEventInfo,
                                              _In_ SIZE_T cbSize);


    // Consumer API

    // ConnectHubEvent
    //  - connects to Hub Event 'lpName'
    //  - returns regular Event, use regular WaitFor... functions and CloseHandle
    //  - it is also possible to use SetEvent and ResetEvent to drive the client side event if required
    //
    HUBEVENT_API HANDLE WINAPI ConnectHubEventW (DWORD dwDesiredAccess, BOOL bInheritHandle, _In_ LPCWSTR lpName);
    HUBEVENT_API HANDLE WINAPI ConnectHubEventA (DWORD dwDesiredAccess, BOOL bInheritHandle, _In_ LPCSTR lpName);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CreateHubEvent  CreateHubEventW
#define ConnectHubEvent ConnectHubEventW
#else
#define CreateHubEvent  CreateHubEventA
#define ConnectHubEvent ConnectHubEventA
#endif

#endif
