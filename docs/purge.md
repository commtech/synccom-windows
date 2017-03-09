# Purge

Between the hardware FIFO and the driver's software buffers there are multiple places data could be stored, excluding your application code. If you ever need to clear this data and start fresh, there are a couple of methods you can use.

An `SYNCCOM_PURGE_RX` is required after changing the `MODE` bits in the `CCR0` register. If you need to change the `MODE` bits but don't have a clock present, change the `CM` bits to `0x7` temporarily. This will give you an internal clock to switch modes. You can then switch to your desired `CM` now that your `MODE` is locked in.

###### Support
| Code | Version |
| ---- | ------- |
| synccom-windows | 1.0.0 |


## Execute
```c
SYNCCOM_PURGE_TX
SYNCCOM_PURGE_RX
```

| System Error | Value | Cause |
| ------------ | -----:| ----- |
| `ERROR_SEM_TIMEOUT` | 121 (0x79) | Command timed out (missing clock) |

###### Examples
Purge the transmit data.
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_PURGE_TX,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```

Purge the receive data.
```c
#include <synccom.h>
...

DeviceIoControl(h, SYNCCOM_PURGE_RX,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/purge.c`](https://github.com/commtech/synccom-windows/blob/master/examples/purge/purge.c)