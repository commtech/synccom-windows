/*
Copyright (C) 2016  Commtech, Inc.

This file is part of synccom-windows.

synccom-windows is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published bythe Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.

synccom-windows is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

You should have received a copy of the GNU General Public License along
with synccom-windows.  If not, see <http://www.gnu.org/licenses/>.
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

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS SelectInterfaces(_In_ struct synccom_port *port);
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS OsrFxSetPowerPolicy(_In_ struct synccom_port *port);


#endif
