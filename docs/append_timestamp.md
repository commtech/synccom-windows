# Append Timestamp
Because of the nature of USB communication, this timestamp value will be wildly inaccurate, but it can be used to verify the order in which frames occurred, or if there was a large gap in time between two frames.

[`KeQuerySystemTime`](http://msdn.microsoft.com/en-us/library/windows/hardware/ff553068.aspx) is used to acquire the timestamp upon complete reception of a frame.
This data will be appended to the end of your frame.

###### Support
| Code | Version |
| ---- | ------- |
| synccom-windows | 1.0.0 |


## Get
```c
SYNCCOM_GET_APPEND_TIMESTAMP
```

###### Examples
```c
#include <synccom.h>
...

unsigned timestamp;

DeviceIoControl(h, SYNCCOM_GET_APPEND_TIMESTAMP,
                NULL, 0,
                &timestamp, sizeof(timestamp),
                &temp, NULL);
```


## Enable
```c
SYNCCOM_ENABLE_APPEND_TIMESTAMP
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_ENABLE_APPEND_TIMESTAMP,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
SYNCCOM_DISABLE_APPEND_TIMESTAMP
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_DISABLE_APPEND_TIMESTAMP,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/append-timestamp.c`](../examples/append-timestamp.c)