/*
Copyright 2019 Commtech, Inc.

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

#include "driver.h"
#include "device.h"

#if defined(EVENT_TRACING)
#include "driver.tmh"
#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, OsrFxEvtDriverContextCleanup)
#endif

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG       config;
    NTSTATUS                status;
    WDF_OBJECT_ATTRIBUTES   attributes;

    WPP_INIT_TRACING( DriverObject, RegistryPath );
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "Sync Com Driver\n");
    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "Built %s %s\n", __DATE__, __TIME__);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "Copyright (c) 2023, Fastcom.");
  
	WDF_DRIVER_CONFIG_INIT(&config, SyncComEvtDeviceAdd);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = OsrFxEvtDriverContextCleanup;

    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfDriverCreate failed with status 0x%x\n", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &port_num_spinlock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate in DriverEntry failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    return status;
}

VOID OsrFxEvtDriverContextCleanup(WDFOBJECT Driver)
{
    PAGED_CODE ();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT,  "%s: Entering.\n", __FUNCTION__);

    WPP_CLEANUP( WdfDriverWdmGetDriverObject( (WDFDRIVER)Driver ));
    UNREFERENCED_PARAMETER(Driver);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "%s: Exiting.\n", __FUNCTION__);
}



