#include <Windows.h>
#include <signal.h>
#include <stdio.h>

#include "..\win32-hub-events.h"

HANDLE hQuit;
HHEVENT hHubEvent;

void signal_handler (int) {
    SetEvent (hQuit);
}

int wmain (int argc, const wchar_t ** argw) {
    signal (SIGINT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGBREAK, signal_handler);

    hQuit = CreateEvent (NULL, TRUE, FALSE, NULL);
    hHubEvent = CreateHubEventW (NULL, FALSE, FALSE, (argc > 1) ? argw [1] : L"Example Hub Event Object");

    if (!hHubEvent) {
        printf ("CreateHubEvent failed, error %u\n", GetLastError ());
        return GetLastError ();
    }

    while (true)
        switch (WaitForSingleObject (hQuit, 1000)) {

            case WAIT_TIMEOUT:
                if (SetHubEvent (hHubEvent)) {

                    HUB_EVENT_INFO info;
                    GetHubEventInfo (hHubEvent, &info, sizeof info);

                    printf ("Signalled %u clients, had %u total!\n",
                            (unsigned) info.nConsumersCount, (unsigned) info.nConsumersTotal);
                } else {
                    printf ("Failed to signal, error %u\n", GetLastError ());
                }
                break;

            case WAIT_OBJECT_0 + 0:
                printf ("Cancelled.\n");
                return 0;

            default:
            case WAIT_FAILED:
                printf ("Failed, error %u\n", GetLastError ());
                return GetLastError ();
        }
    
    return 0;
}
