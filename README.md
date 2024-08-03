# Win32 Hub Events
*Downlevel (suboptimal) sample implementation of proposed Hub Events*

## Description

The Hub Event is similar to the standard [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects),
but multiple consumers (clients) of the event can reliably
[wait](https://learn.microsoft.com/en-us/windows/win32/sync/wait-functions) on it.
Use it, if you need to reliably release all waiting threads, each exactly once.

## The problem being solved here

If multiple threads are waiting on a standard event object,
a [SetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent)
will either:

a) release exactly one (when the event is auto-reset) waiting thread, or  
b) some number waiting threads some number of times (when the event is manual-reset) until 
[ResetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-resetevent) is called.

The [PulseEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-pulseevent) attempts
to release all waiting threads exactly once, but is very unreliable in both nature and implementation.

## Solution using Hub Events

When producer (server) uses [SetHubEvent]() to signal an auto-reset event,
all waiting threads are released exactly once.

That's acomplished by every [ConnectHubEvent](win32-hub-events.cpp#L137) producing distinct
standard [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects),
that the application can also Set/Reset/Pulse on it's own.
Such actions will not affect the producer or other consumers.

## API

**Producer:**

* [CreateHubEvent(A/W)](win32-hub-events.cpp#L41) - creates server side of Hub Event, used by following APIs:
* [DestroyHubEvent](win32-hub-events.cpp#L61) - stops the server and frees all resources
* [SetHubEvent](win32-hub-events.cpp#L88) - sets events for all consumers (clients)
* [ResetHubEvent](win32-hub-events.cpp#L91) - resets events for all consumers (clients)
* [GetHubEventInfo](win32-hub-events.h#L64) - retrieves various information and statistics about the Hub Event

**Consumer:**

* [ConnectHubEvent(A/W)](win32-hub-events.h#L76) - connects to a Hub Event to receive set/reset state

The API returns regular [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects) handle
which can be used interchangeably, as if returned by CreateEvent or OpenEvent.

* [All standard wait functions](https://learn.microsoft.com/en-us/windows/win32/sync/wait-functions).
* [SetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent)
* [ResetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-resetevent)
* [PulseEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-pulseevent)
* [CloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle)

## Remarks

* Server side (producer) is implemented as [Named Pipe](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipes)
  server that hands out separate [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects)
  to each client (consumer).
* The purpose of this example is to ilustrate a feature that should be provided by the OS as a kernel object alongside 
  [events](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects),
  [mutexes](https://learn.microsoft.com/en-us/windows/win32/sync/mutex-objects),
  [semaphores](https://learn.microsoft.com/en-us/windows/win32/sync/semaphore-objects), etc.

