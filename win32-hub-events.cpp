#ifndef HUBEVENT_API
#define HUBEVENT_API __declspec(dllexport)
#endif

#include "win32-hub-events.h"
#include <winternl.h>
#include <stdio.h>

#pragma warning (disable:6053) // _sn(w)print may not NUL-terminate
#pragma warning (disable:28159) // GetTickCount

struct HubEvent : OVERLAPPED {
    HANDLE    hPipe;
    TP_WAIT * tpWait;
    HANDLE *  hEvents;
    SRWLOCK   lockEvents;
    SIZE_T    nConnectCounter;
    bool      bManualReset;
    bool      bInitialState;
    bool      bEmptySlot; // there may be empty slot in 'hEvents'
    DWORD     dwLastPrune;
    SIZE_T    nPrunedCount;
};

namespace {
    const DWORD dwPipeOpenMode = FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
    const DWORD dwPipePipeMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS;
    const DWORD dwPipeBufferSize = sizeof (WCHAR) * MAX_PATH;
    const DWORD dwPipeTimeout = 250;
    const DWORD dwPruneTimeout = 2500;
    const DWORD maskInheritHandle = 0x0000'1000u; // any bit not present in 'EVENT_ALL_ACCESS'

    HubEvent * InitAndStart (HANDLE hPipe, bool bManualReset, bool bInitialState);
    BOOL WINAPI ActHubEvent (_In_ HHEVENT context, BOOL (WINAPI * Action) (_In_ HANDLE));
    void CALLBACK HeapWaitCallback (PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);
    
    void PruneConsumerHandles (HubEvent *);
    void PruneConsumerHandles (HubEvent *, HANDLE, SIZE_T);
}

// CreateHubEvent

HHEVENT WINAPI CreateHubEventW (_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes, BOOL bManualReset, BOOL bInitialState, _In_ LPCWSTR lpName) {
    WCHAR szPipeName [MAX_PATH + 10];
    _snwprintf (szPipeName, MAX_PATH + 10, L"\\\\.\\pipe\\%s", lpName);

    return InitAndStart (CreateNamedPipeW (szPipeName, dwPipeOpenMode, dwPipePipeMode, 1,
                                           dwPipeBufferSize, dwPipeBufferSize, dwPipeTimeout, lpSecurityAttributes),
                         bManualReset, bInitialState);
}

HHEVENT WINAPI CreateHubEventA (_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes, BOOL bManualReset, BOOL bInitialState, _In_ LPCSTR lpName) {
    CHAR szPipeName [MAX_PATH + 10];
    _snprintf (szPipeName, MAX_PATH + 10, "\\\\.\\pipe\\%s", lpName);

    return InitAndStart (CreateNamedPipeA (szPipeName, dwPipeOpenMode, dwPipePipeMode, 1,
                                           dwPipeBufferSize, dwPipeBufferSize, dwPipeTimeout, lpSecurityAttributes),
                         bManualReset, bInitialState);
}

// DestroyHubEvent

BOOL WINAPI DestroyHubEvent (_In_ HHEVENT context) {
    SetThreadpoolWait (context->tpWait, NULL, NULL);
    WaitForThreadpoolWaitCallbacks (context->tpWait, TRUE);
    CloseThreadpoolWait (context->tpWait);

    DisconnectNamedPipe (context->hPipe);
    CloseHandle (context->hPipe);
    CloseHandle (context->hEvent);

    HANDLE heap = GetProcessHeap ();
    SIZE_T size = HeapSize (heap, 0, context->hEvents) / sizeof (HANDLE);

    for (SIZE_T i = 0; i != size; ++i) {
        if (context->hEvents [i]) {
            CloseHandle (context->hEvents [i]);
        }
    }

    HeapFree (heap, 0, context->hEvents);
    HeapFree (heap, 0, context);
    return TRUE;
}

// Set/Reset/Pulse

BOOL WINAPI SetHubEvent (_In_ HHEVENT context) {
    return ActHubEvent (context, SetEvent);
}
BOOL WINAPI ResetHubEvent (_In_ HHEVENT context) {
    return ActHubEvent (context, ResetEvent);
}
BOOL WINAPI PulseHubEvent (_In_ HHEVENT context) {
    return ActHubEvent (context, PulseEvent);
}

// GetHubEventInfo

BOOL WINAPI GetHubEventInfo (_In_ HHEVENT context,
                             _Out_writes_bytes_ (cbSize) PHUB_EVENT_INFO lpHubEventInfo,
                             _In_ SIZE_T cbSize) {
    AcquireSRWLockShared (&context->lockEvents);

    HANDLE heap = GetProcessHeap ();
    SIZE_T size = HeapSize (heap, 0, context->hEvents) / sizeof (HANDLE);

    PruneConsumerHandles (context, heap, size);

#define FIELD_REACHABLE(field) cbSize >= offsetof (HUB_EVENT_INFO, field) + sizeof (HUB_EVENT_INFO::field)

    if (FIELD_REACHABLE (nConsumersCount)) {
        SIZE_T n = 0;
        for (SIZE_T i = 0; i != size; ++i) {
            if (context->hEvents [i]) {
                ++n;
            }
        }
        lpHubEventInfo->nConsumersCount = n;
    }
    if (FIELD_REACHABLE (nConsumersTotal)) lpHubEventInfo->nConsumersTotal = context->nConnectCounter;
    if (FIELD_REACHABLE (bManualReset)) lpHubEventInfo->bManualReset = context->bManualReset;
    if (FIELD_REACHABLE (bInitialState)) lpHubEventInfo->bInitialState = context->bInitialState;

#undef FIELD_REACHABLE

    ReleaseSRWLockShared (&context->lockEvents);

    // zero out remaining fields

    if (cbSize > sizeof (HUB_EVENT_INFO)) {
        memset (lpHubEventInfo + 1, 0, cbSize - sizeof (HUB_EVENT_INFO));
    }
    return TRUE;
}

// ConnectHubEvent (A/W)
//  - the server side is implemented as named pipe server

HANDLE WINAPI ConnectHubEventW (DWORD dwDesiredAccess, BOOL bInheritHandle, _In_ LPCWSTR lpName) {
    WCHAR szPipeName [MAX_PATH + 10];
    _snwprintf (szPipeName, MAX_PATH + 10, L"\\\\.\\pipe\\%s", lpName);

    if (bInheritHandle) {
        dwDesiredAccess |= maskInheritHandle;
    }

    DWORD nRead = 0;
    HANDLE hEvent = NULL;
    if (CallNamedPipeW (szPipeName, &dwDesiredAccess, sizeof dwDesiredAccess, &hEvent, sizeof hEvent, &nRead, dwPipeTimeout) || (GetLastError () == ERROR_MORE_DATA)) {
        return hEvent;
    } else
        return NULL;
}

HANDLE WINAPI ConnectHubEventA (DWORD dwDesiredAccess, BOOL bInheritHandle, _In_ LPCSTR lpName) {
    CHAR szPipeName [MAX_PATH + 10];
    _snprintf (szPipeName, MAX_PATH + 10, "\\\\.\\pipe\\%s", lpName);

    if (bInheritHandle) {
        dwDesiredAccess |= maskInheritHandle;
    }

    DWORD nRead = 0;
    HANDLE hEvent = NULL;
    if (CallNamedPipeA (szPipeName, &dwDesiredAccess, sizeof dwDesiredAccess, &hEvent, sizeof hEvent, &nRead, dwPipeTimeout) || (GetLastError () == ERROR_MORE_DATA)) {
        return hEvent;
    } else
        return NULL;
}


namespace {
    HubEvent * InitAndStart (HANDLE hPipe, bool bManualReset, bool bInitialState) {
        if (hPipe != INVALID_HANDLE_VALUE) {
            
            HANDLE heap = GetProcessHeap ();
            HubEvent * context = (HubEvent *) HeapAlloc (heap, HEAP_ZERO_MEMORY, sizeof (HubEvent));

            if (context) {
                context->hPipe = hPipe;
                context->bManualReset = bManualReset;
                context->bInitialState = bInitialState;
                context->bEmptySlot = TRUE;
                context->dwLastPrune = GetTickCount ();

                context->hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
                if (context->hEvent) {

                    context->hEvents = (HANDLE *) HeapAlloc (heap, HEAP_ZERO_MEMORY, 4 * sizeof (HANDLE));
                    if (context->hEvents) {

                        context->tpWait = CreateThreadpoolWait (HeapWaitCallback, context, NULL);
                        if (context->tpWait) {

                            SetThreadpoolWait (context->tpWait, context->hEvent, NULL);

                            if (ConnectNamedPipe (hPipe, context) || (GetLastError () == ERROR_IO_PENDING)) {
                                return context;
                            }

                            CloseThreadpoolWait (context->tpWait);
                        }
                        CloseHandle (context->hEvents);
                    }
                    CloseHandle (context->hEvent);
                }
                HeapFree (heap, 0, context);
            }
            CloseHandle (hPipe);
        }
        return NULL;
    }

    BOOL WINAPI ActHubEvent (_In_ HHEVENT context, BOOL (WINAPI * Action) (_In_ HANDLE)) {
        BOOL result = TRUE;
        AcquireSRWLockShared (&context->lockEvents);

        HANDLE heap = GetProcessHeap ();
        SIZE_T size = HeapSize (heap, 0, context->hEvents) / sizeof (HANDLE);

        PruneConsumerHandles (context, heap, size);

        for (SIZE_T i = 0; i != size; ++i) {
            if (context->hEvents [i]) {
                if (!Action (context->hEvents [i])) {
                    result = FALSE;
                }
            }
        }

        ReleaseSRWLockShared (&context->lockEvents);
        return result;
    }

    bool FindAndSetEmptySlot (HANDLE * handles, SIZE_T n, HANDLE handle) {
        for (SIZE_T i = 0; i != n; ++i) {
            if (handles [i] == NULL) {
                handles [i] = handle;
                return true;
            }
        }
        return false;
    }

    void CALLBACK HeapWaitCallback (PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult) {
        HubEvent * context = (HubEvent *) Context;

        DWORD n = 0;
        DWORD dwDesiredAccess = 0;
        bool success = false;

        if (ReadFile (context->hPipe, &dwDesiredAccess, sizeof dwDesiredAccess, &n, NULL) && (n == sizeof (DWORD))) {

            HANDLE hNewEvent = CreateEventW (NULL, context->bManualReset, context->bInitialState, NULL);
            if (hNewEvent) {

                DWORD dwClientProcessId = 0;
                if (GetNamedPipeClientProcessId (context->hPipe, &dwClientProcessId)) {

                    HANDLE hClientProcess = OpenProcess (PROCESS_DUP_HANDLE, FALSE, dwClientProcessId);
                    if (hClientProcess) {

                        BOOL bInheritHandle = !!(dwDesiredAccess & maskInheritHandle);
                        dwDesiredAccess &= ~maskInheritHandle;

                        HANDLE hClientEvent = NULL;
                        if (DuplicateHandle (GetCurrentProcess (), hNewEvent,
                                             hClientProcess, &hClientEvent,
                                             dwDesiredAccess, bInheritHandle, 0)) {

                            if (WriteFile (context->hPipe, &hClientEvent, sizeof hClientEvent, &n, NULL)) {

                                AcquireSRWLockExclusive (&context->lockEvents);

                                HANDLE heap = GetProcessHeap ();
                                SIZE_T size = HeapSize (heap, 0, context->hEvents) / sizeof (HANDLE);

                                PruneConsumerHandles (context, heap, size);

                                if (context->bEmptySlot) {
                                    if (FindAndSetEmptySlot (context->hEvents, size, hNewEvent)) {
                                        success = true;
                                    } else {
                                        context->bEmptySlot = FALSE;
                                    }
                                }

                                if (!context->bEmptySlot) {
                                    HANDLE * newEvents = (HANDLE *) HeapReAlloc (heap, 0, context->hEvents, (size + 1) * sizeof (HANDLE));
                                    if (newEvents) {
                                        context->hEvents = newEvents;
                                        context->hEvents [size] = hNewEvent;
                                        success = true;
                                    }
                                }

                                // delay next pruning so that we don't delete the event before consumer opens it
                                context->dwLastPrune = GetTickCount ();
                                context->nConnectCounter++;

                                ReleaseSRWLockExclusive (&context->lockEvents);
                            }
                        }
                        CloseHandle (hClientProcess);
                    }
                }

                if (!success) {
                    CloseHandle (hNewEvent);
                }
            }
        }
        
        FlushFileBuffers (context->hPipe);
        DisconnectNamedPipe (context->hPipe);

        // wait for next request

        ResetEvent (context->hEvent);
        SetThreadpoolWait (context->tpWait, context->hEvent, NULL);
        ConnectNamedPipe (context->hPipe, context);
    }

    void PruneConsumerHandles (HubEvent * context) {
        HANDLE heap = GetProcessHeap ();
        return PruneConsumerHandles (context, heap, HeapSize (heap, 0, context->hEvents) / sizeof (HANDLE));
    }

    void PruneConsumerHandles (HubEvent * context, HANDLE heap, SIZE_T n) {
        DWORD now = GetTickCount ();
        if (now - context->dwLastPrune > dwPruneTimeout) {
            context->dwLastPrune = now;

            for (SIZE_T i = 0; i != n; ++i) {
                if (context->hEvents [i] != NULL) {

                    PUBLIC_OBJECT_BASIC_INFORMATION info;
                    if (NtQueryObject (context->hEvents [i], ObjectBasicInformation, &info, sizeof info, NULL) == 0) {

                        if (info.HandleCount < 2) {
                            CloseHandle (context->hEvents [i]);
                            context->hEvents [i] = NULL;
                            context->bEmptySlot = true;
                        }
                    }
                }
            }
        }
    }
}
