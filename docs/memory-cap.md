# Memory Cap

If your system has limited memory available, there are safety checks in place to prevent spurious incoming data from overrunning your system. Each port has an option for setting it's input and output memory cap.

###### Support
| Code  | Version |
| ----- | ------- |
| synccom-windows | 1.0.0 |


## Structure
```c
struct synccom_memory_cap {
    int input;
    int output;
};
```


## Macros
```c
SYNCCOM_MEMORY_CAP_INIT(memcap)
```

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| `memcap` | `struct synccom_memory_cap *` | The memory cap structure to initialize |

The `SYNCCOM_MEMORY_CAP_INIT` macro should be called each time you use the  `struct synccom_memory_cap` structure. An initialized structure will allow you to only set/receive the memory cap you need.


## Get
```c
SYNCCOM_GET_MEMORY_CAP
```

###### Examples
```
#include <synccom.h>
...

struct synccom_memory_cap memcap;

SYNCCOM_MEMORY_CAP_INIT(memcap);

DeviceIoControl(port, SYNCCOM_GET_MEMORY_CAP,
                NULL, 0,
                &memcap, sizeof(memcap),
                &tmp, (LPOVERLAPPED)NULL);
```

At this point `memcap.input` and `memcap.output` would be set to their respective values.


## Set
```c
SYNCCOM_SET_MEMORY_CAP
```

###### Examples
```
#include <synccom.h>
...

struct synccom_memory_cap memcap;

SYNCCOM_MEMORY_CAP_INIT(memcap);

memcap.input = 1000000; /* 1 MB */
memcap.output = 1000000; /* 1 MB */

DeviceIoControl(port, SYNCCOM_SET_MEMORY_CAP,
                &memcap, sizeof(memcap),
                NULL, 0,
                &tmp, (LPOVERLAPPED)NULL);
```


### Additional Resources
- Complete example: [`examples/memory-cap.c`](../examples/memory-cap.c)