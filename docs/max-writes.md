# Max Writes

The nature of USB means it is sometimes necessary to queue up many consecutive data transmits to the Sync Com to try and achieve smaller gaps between transmitted frames. However, there needs to be a limit to the number of pending transfers to the Sync Com. This sets the maximum number of pending writes to the Sync Com. Increasing this number may decrease latency between frames, but also increase memory usage. 

###### Support
| Code | Version |
| ---- | ------- |
| synccom-windows | 2.2.0.0 |


## Get
```c
SYNCCOM_GET_TX_MODIFIERS
```

###### Examples
```
#include <synccom.h>
...

unsigned modifiers;

DeviceIoControl(h, SYNCCOM_GET_TX_MODIFIERS,
				NULL, 0,
				&modifiers, sizeof(modifiers),
				&temp, NULL);
```


## Set
```c
SYNCCOM_SET_MAX_WRITES
```

###### Examples
```c
#include <synccom.h>
...
unsigned max_writes;

DeviceIoControl(h, SYNCCOM_SET_MAX_WRITES,
				NULL, 0,
				&max_writes, sizeof(max_writes),
				&temp, NULL);
```


### Additional Resources
- Complete example: [`examples/max-writes.c`](https://github.com/commtech/synccom-windows/blob/master/examples/max-writes/max-writes.c)