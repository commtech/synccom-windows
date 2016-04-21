# synccom-windows
This README file is best viewed [online](http://github.com/commtech/synccom-windows/).

## Installing Driver

##### Downloading Driver Package
You can download a pre-built driver package directly from our [website](http://www.commtech-fastcom.com/CommtechSoftware.html).

## Quick Start Guide
There is documentation for each specific function listed below, but lets get started with a quick programming example for fun.

_This tutorial has already been set up for you at_ [`synccom-windows/exe/tutorial/tutorial.c`](exe/tutorial/tutorial.c).


Create a new C file (named tutorial.c) with the following code.

```c
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

int main(void)
{
	HANDLE h = 0;
	DWORD tmp;
	char odata[] = "Hello world!";
	char idata[20];

	/* Open port 0 in a blocking IO mode */
	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                  OPEN_EXISTING, 0, NULL);

	if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		return EXIT_FAILURE;
	}

	/* Send "Hello world!" text */
	WriteFile(h, odata, sizeof(odata), &tmp, NULL);

	/* Read the data back in (with our loopback connector) */
	ReadFile(h, idata, sizeof(idata), &tmp, NULL);

	fprintf(stdout, "%s\n", idata);

	CloseHandle(h);

	return EXIT_SUCCESS;
}

```

For this example I will use the Visual Studio command line compiler, but you can use your compiler of choice.

```
# cl tutorial.c
```

Now attach the included loopback connector.

```
# tutorial.exe
Hello world!
```

You have now transmitted and received an HDLC frame!


## API Reference

There are likely other configuration options you will need to set up for your  own program. All of these options are described on their respective documentation page.

- [Connect](docs/connect.md)
- [Append Status](docs/append-status.md)
- [Append Timestamp](docs/append-timestamp.md)
- [Clock Frequency](docs/clock-frequency.md)
- [Memory Cap](docs/memory-cap.md)
- [Purge](docs/purge.md)
- [Read](docs/read.md)
- [Registers](docs/registers.md)
- [RX Multiple](docs/rx-multiple.md)
- [TX Modifiers](docs/tx-modifiers.md)
- [Write](docs/write.md)
- [Disconnect](docs/disconnect.md)

### FAQ

##### Why does executing a purge without a clock put the card in a broken state?
When executing a purge on either the transmitter or receiver there is a TRES or RRES (command from the CMDR register) happening behind the scenes. If there isn't a clock available, the command will stall until a clock is available. This should work in theory but doesn't in practice. So whenever you need to execute a purge without a clock: first put it into clock mode 7, execute your purge, and then return to your other clock mode.

##### The CRC-CCITT generated is not what I expected.
There are many resources online that say they can calculate CRC-CCITT, but most don't use the correct formula defined by the HDLC specification.

An example of one that doesn't is [lammertbies.nl](http://www.lammertbies.nl/comm/info/crc-calculation.html) and the [forum post](http://www.lammertbies.nl/forum/viewtopic.php?t=607) explaining why it isn't generated correctly.

We recommend using this [CRC generator](http://www.zorc.breitbandkatze.de/crc.html) to calculate the correct value and to use [these settings](http://i.imgur.com/G6zT87i.jpg).


##### What does each LED color mean?
| Color | Version |
| ----- | ------- |
| `Red` | Receive line |
| `Green or yellow` | Transmit line |


These are not bi-color LEDs, they are only red and only green (or yellow).

##### Why are there extra bits surrounding the data while using RS485?
When operating in RS-485 mode the physical driver chip is in a disabled state when data is not being actively transmitted. It is necessary to re-enable the driver prior to the start of data transmission. This is to ensure that the driver is fully enabled and ready to receive data when the first bit of data arrives. Likewise it is necessary for the driver to remain enabled for a brief time after the last bit of data has been transmitted. This is to ensure that the data has completely propagated through the driver before it is disabled. The FCore will enable and disable the RS-422 driver chip one clock period before the first bit of data and one clock period after the last bit of data. As a result you will see an extra 1 bit (if idling ones) appear just before the first bit of your data and just after the last bit of your data. This is not erroneous data bit and will be outside of your normal data frame and will not be confused as valid data.

##### How long after CTS until the data is transmitted?
Transmission happens within a fixed time frame after CTS of around 1 - 1.5 clock cycles.

##### How do I get past the 'Windows Logo testing' check?

1. Right-click on My Computer and click Properties
2. Click the Hardware tab, then click Driver Signing under Drivers
3. Select 'Warn: Prompt me each time to choose an action'

Once you're done, restart the workstation and install the driver as usual. Windows shouldn't bother you about logo testing this time.


##### How do I receive parse raw data without any framing or sync signal?
The difficulty in using transparent mode without a data sync signal, is that you have no idea where valid receive data starts and stops.  You must shift through the bit stream until you find valid data.  Once you have found valid data and noted the correct bit alignment, if the data reception remains constant, you should be able to shift all incoming data by the correct number of bits and you will then have correct data.

Lets start by sending `0xa5a5` (`10100101` `10100101`) with LSB being transmitted first.
`10100101` `10100101`

Now the idle is 1's so the message will be received (depending on how many 1's are clocked in prior to the actual data) the possibilities are:
```
11111111 10100101 10100101 11111111
1111111 10100101 10100101 111111111
111111 10100101 10100101 1111111111
11111 10100101 10100101 11111111111
1111 10100101 10100101 111111111111
111 10100101 10100101 1111111111111
11 10100101 10100101 11111111111111
1 10100101 10100101 111111111111111
```

Re-aligning this to byte boundaries:
```
11111111 10100101 10100101 11111111
11111111 01001011 01001011 11111111
11111110 10010110 10010111 11111111
11111101 00101101 00101111 11111111
11111010 01011010 01011111 11111111
11110100 10110100 10111111 11111111
11101001 01101001 01111111 11111111
11010010 11010010 11111111 11111111
```

Remembering that LSB was transmitted first, changing it to "normal" (MSB) gives possible received messages
```
11111111 10100101 10100101 11111111 = 0xffa5a5ff
11111111 11010010 11010010 11111111 = 0xffd2d2ff
01111111 01101001 11101001 11111111 = 0x7f69e9ff
10111111 10110100 11110100 11111111 = 0xbfb4f4ff
01011111 01011010 11111010 11111111 = 0x5f5afaff
00101111 00101101 11111101 11111111 = 0x2f2dfdff
10010111 10010110 11111110 11111111 = 0x9796feff
01001011 01001011 11111111 11111111 = 0x4b4bffff
```

They are all representative of the original 0xa5a5 that was sent, however only 1 of them directly shows that upon receive without shifting.

And once you determine how many bits you need to shift to get valid data, everything that you read in will need to be shifted by that number of bits until you stop transmitting data.  If you stop transmitting, then you have to do the same thing over again once you start transmitting data again.

This can be avoided by utilizing a receive data strobe in an appropriate clock mode.  If the transmitting device can supply a strobe signal that activates at the beginning of the of the data and deactivates at the end, the receiver will only be activated during the active phase of this signal, and hopefully the data will have the correct alignment.


##### How do I upgrade driver versions?
1. Open the 'Device Manager'
3. Right click & select 'Properties' on each Commtech port
4. Switch to the 'Driver' tab
5. Click on the 'Update Driver...' button
6. Then browse to the location of the new driver files


##### How do I build a custom version of the driver source code?
The source code for the Fastcom FSCC driver is hosted on Github code hosting. To check out the latest code you will need Git and to run the following command in a terminal:

```
git clone git://github.com/commtech/synccom-windows.git synccom
```

We prefer you use the above method for downloading the driver source code (because it is the easiest way to stay up to date), but you can also get the driver source code from the [download page](https://github.com/commtech/synccom-windows/tags/).

Now that you have the latest code checked out, you will probably want to switch to a stable version within the code directory. You can do this by browsing the various tags for one you would like to switch to. Version v2.2.8 is only listed here as an example.

```
git tag
git checkout v2.2.8
```

###### Status Bytes
Getting the frame status has now been designed to be configurable. When using the 1.x driver, you will always have the frame status appended to your data on a read. When using the 2.x driver, this can be toggled and defaults to not appending the status to the data.

All of the 2.2.X releases will not break API compatability. If a function in the 2.2.X series returns an incorrect value, it can be fixed to return the correct value in a later release.

When and if we switch to a 2.3 release there will only be minor API changes.

## Build Dependencies
- Windows Driver Kit (7.1.0 used internally to support XP)


## Run-time Dependencies
- OS: Windows 7+


## API Compatibility
We follow [Semantic Versioning](http://semver.org/) when creating releases.


## License

Copyright (C) 2016 [Commtech, Inc.](http://fastcomproducts.com)

Licensed under the [GNU General Public License v3](http://www.gnu.org/licenses/gpl.txt).

