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

#ifndef SYNCCOM_DEVICE_H
#define SYNCCOM_DEVICE_H
#include <ntddk.h>
#include <wdf.h>
#include <defines.h>
#include "trace.h"

struct synccom_port *synccom_port_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit);

EVT_WDF_DRIVER_DEVICE_ADD SyncComEvtDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY SyncComEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT SyncComEvtDeviceD0Exit;
EVT_WDF_DEVICE_PREPARE_HARDWARE SyncComEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE SyncComEvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_WRITE SyncComEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_READ SyncComEvtIoRead;

NTSTATUS setup_spinlocks(_In_ struct synccom_port *port);
NTSTATUS setup_dpc(_In_ struct synccom_port *port);
NTSTATUS setup_memory(_In_ struct synccom_port *port);
NTSTATUS setup_request(_In_ struct synccom_port *port);
NTSTATUS setup_timer(_In_ struct synccom_port *port);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS SelectInterfaces(_In_ struct synccom_port *port);
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS OsrFxSetPowerPolicy(_In_ struct synccom_port *port);


#endif
