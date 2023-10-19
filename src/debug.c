/*
Copyright 2023 Commtech, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#include "debug.h"
#include "utils.h" /* return_{val_}if_true */

#if defined(EVENT_TRACING)
#include "debug.tmh"
#endif

#if !defined(EVENT_TRACING)
VOID
TraceEvents(
	IN TRACEHANDLE TraceEventsLevel,
	IN ULONG   TraceEventsFlag,
	IN PCCHAR  DebugMessage,
	...
)
{
#if DBG
#define     TEMP_BUFFER_SIZE        1024

	va_list    list;
	CHAR      debugMessageBuffer[TEMP_BUFFER_SIZE];
	NTSTATUS   status;

	UNREFERENCED_PARAMETER(TraceEventsFlag);

	va_start(list, DebugMessage);

	if (DebugMessage) {

		//
		// Using new safe string functions instead of _vsnprintf.
		// This function takes care of NULL terminating if the message
		// is longer than the buffer.
		//
		status = RtlStringCbVPrintfA(debugMessageBuffer,
			sizeof(debugMessageBuffer),
			DebugMessage,
			list);
		if (!NT_SUCCESS(status)) {

			KdPrint(("synccom-windows.sys: RtlStringCbVPrintfA failed %x\n", status));
			return;
		}
		if (TraceEventsLevel < TRACE_LEVEL_INFORMATION) {

			KdPrint((debugMessageBuffer));
		}
	}
	va_end(list);

	return;

#else

	UNREFERENCED_PARAMETER(TraceEventsLevel);
	UNREFERENCED_PARAMETER(TraceEventsFlag);
	UNREFERENCED_PARAMETER(DebugMessage);
#endif
}

#endif
