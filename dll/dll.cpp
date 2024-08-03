#include <Windows.h>

BOOL APIENTRY DllMain (HMODULE, DWORD reason, LPVOID global) {
    switch (reason) {
        case DLL_PROCESS_DETACH:
            if (global) {
                // static unload
            } else {
                // dynamic unload
            }
            break;
    }
    return TRUE;
}
