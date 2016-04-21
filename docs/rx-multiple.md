# RX Multiple

RX Multiple allows you to return multiple frames from the FIFO at once to user-space. While active, every read will return N frames, where the size of all N frames combined is less than the 'size' passed into the read function.

For example, if you have 10 frames of 128 size in your input buffer and have RX Multiple on, when you activate a read with a size of 4096, all 10 frame will be placed into the variable supplied to the read function.

###### Support
| Code | Version |
| ---- | -------- |
| synccom-windows | 1.0.0 |


## Get
```c
SYNCCOM_GET_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

unsigned status;

DeviceIoControl(h, SYNCCOM_GET_RX_MULTIPLE,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
SYNCCOM_ENABLE_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_ENABLE_RX_MULTIPLE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
SYNCCOM_DISABLE_RX_MULTIPLE
```

###### Examples
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_DISABLE_RX_MULTIPLE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/rx-multiple.c`](../examples/rx-multiple.c)