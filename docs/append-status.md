# Append Status

It is a good idea to pay attention to the status of each frame. For example, you may want to see if the frame's CRC check succeeded or failed.

The Sync Com reports this data to you by appending two additional bytes to each frame you read from the card, if you opt-in to see this data. There are a few methods of enabling this additional data.

These two bytes represent the STAR register. (page 67 of the manual)

###### Support
| Code | Version |
| ---- | ------- |
| synccom-windows | 1.0.0 |


## Get
```c
SYNCCOM_GET_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

unsigned status;

DeviceIoControl(h, SYNCCOM_GET_APPEND_STATUS,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
SYNCCOM_ENABLE_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_ENABLE_APPEND_STATUS,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
SYNCCOM_DISABLE_APPEND_STATUS
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_DISABLE_APPEND_STATUS,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/append-status.c`](https://github.com/commtech/synccom-windows/blob/master/examples/append-status/append-status.c)