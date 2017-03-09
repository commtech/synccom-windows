# Read

###### Support
| Code | Version |
| ---- | ------- |
| synccom-windows | 2.0.0 |


## Read
The Windows [`ReadFile`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467.aspx) is used to read data from the port.

| System Error | Value | Cause |
| ------------ | -----:| ----- |
| `ERROR_INSUFFICIENT_BUFFER` | 122 (0x7A) | The buffer size is smaller than the next frame |

###### Examples
```c
#include <synccom.h>
...

char idata[20] = {0};
unsigned bytes_read;

ReadFile(h, idata, sizeof(idata), (DWORD*)bytes_read, NULL);
```


### Additional Resources
- Complete example: [`examples/tutorial.c`](https://github.com/commtech/synccom-windows/blob/master/examples/tutorial/tutorial.c)