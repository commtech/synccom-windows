/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    private.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#ifndef SYNCCOM_DRIVER_H
#define SYNCCOM_DRIVER_H
#include <initguid.h>
#include <ntddk.h>
#include "usbdi.h"
#include "usbdlib.h"
#include "driverspecs.h"
#include <wdf.h>
#include <wdfusb.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <defines.h>
#include "trace.h"

//
// Include auto-generated ETW event functions (created by MC.EXE from 
// osrusbfx2.man)
//
#include "fx2Events.h"

#define POOL_TAG (ULONG) 'FNYS'
#define _DRIVER_NAME_ "SYNCCOM"
extern ULONG DebugLevel;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP OsrFxEvtDriverContextCleanup;

#endif