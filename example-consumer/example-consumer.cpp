#include <Windows.h>
#include <signal.h>
#include <stdio.h>

#include "..\win32-hub-events.h"

static union {
    struct {
        HANDLE hQuit;
        HANDLE hEvent;
    };
    HANDLE handles [2];
};

void signal_handler (int) {
    SetEvent (hQuit);
}

int wmain (int argc, const wchar_t ** argw) {
    signal (SIGINT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGBREAK, signal_handler);

    hQuit = CreateEvent (NULL, TRUE, FALSE, NULL);
    hEvent = ConnectHubEventW (SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
                               (argc > 1) ? argw [1] : L"Example Hub Event Object");

    if (!hEvent) {
        printf ("ConnectHubEvent failed, error %u\n", GetLastError ());
        return GetLastError ();
    }

    while (true)
        switch (WaitForMultipleObjects (sizeof handles / sizeof handles [0], handles, FALSE, 250)) {

            case WAIT_OBJECT_0 + 1:
                printf ("SIGNAL!\n");
                break;

            case WAIT_TIMEOUT:
                printf (".");
                break;

            case WAIT_OBJECT_0 + 0:
                printf ("Quit.\n");
                return 0;

            default:
            case WAIT_FAILED:
                printf ("Failed, error %u\n", GetLastError ());
                return GetLastError ();
        }
}
