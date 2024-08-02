# Win32 Hub Events
*Downlevel (suboptimal) sample implementation of proposed Hub Events*

## Description

The Hub Event is similar to the standard [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects),
but multiple consumers (clients) of the event can reliably
[wait](https://learn.microsoft.com/en-us/windows/win32/sync/wait-functions) on it.

## The problem being solved here

If multiple threads are waiting on a standard event object,
a [SetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent)
will either:
a) release exactly one (when the event is auto-reset) waiting thread,
b) any number waiting threads any number of times (when the event is manual-reset) until 
[ResetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-resetevent) is called.

The [PulseEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-pulseevent) attempts
to release all waiting threads exactly once, but is very unreliable in both nature and implementation.

## Solution using Hub Events

When producer (server) uses [SetHubEvent]() to signal an auto-reset event,
all waiting threads are released exactly once.

That's acomplished by every [OpenHubEvent]() producing distinct
standard [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects),
that the application can also Set/Reset/Pulse on it's own.
Such actions will not affect the producer or other consumers.

## API

**Producer:**

* [CreateHubEvent(A/W)]()
* [DestroyHubEvent]()
* [SetHubEvent]()
* [ResetHubEvent]()
* [GetHubEventInfo]()

**Consumer:**

* [ConnectHubEvent(A/W)]()

The API returns regular [Event Object](https://learn.microsoft.com/en-us/windows/win32/sync/event-objects) handle
which can be used interchangeably, as if returned by CreateEvent or OpenEvent.

* [All standard wait functions](https://learn.microsoft.com/en-us/windows/win32/sync/wait-functions).
* [SetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent)
* [ResetEvent](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-resetevent)
* [PulseEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-pulseevent)
* [CloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle)

## Remarks

* aaa

## Links

* aaa

