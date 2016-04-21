/*
Copyright (C) 2014  Commtech, Inc.
This file is part of fscc-windows.
fscc-windows is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published bythe Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.
fscc-windows is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.
You should have received a copy of the GNU General Public License along
with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef synccom_H
#define synccom_H

#include <ntddk.h>
#include <wdf.h>

#define synccom_IOCTL_MAGIC 0x8018

#define synccom_GET_REGISTERS CTL_CODE(synccom_IOCTL_MAGIC, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_SET_REGISTERS CTL_CODE(synccom_IOCTL_MAGIC, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_PURGE_TX CTL_CODE(synccom_IOCTL_MAGIC, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_PURGE_RX CTL_CODE(synccom_IOCTL_MAGIC, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_ENABLE_APPEND_STATUS CTL_CODE(synccom_IOCTL_MAGIC, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_DISABLE_APPEND_STATUS CTL_CODE(synccom_IOCTL_MAGIC, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_APPEND_STATUS CTL_CODE(synccom_IOCTL_MAGIC, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_SET_MEMORY_CAP CTL_CODE(synccom_IOCTL_MAGIC, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_MEMORY_CAP CTL_CODE(synccom_IOCTL_MAGIC, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_SET_CLOCK_BITS CTL_CODE(synccom_IOCTL_MAGIC, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_ENABLE_IGNORE_TIMEOUT CTL_CODE(synccom_IOCTL_MAGIC, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_DISABLE_IGNORE_TIMEOUT CTL_CODE(synccom_IOCTL_MAGIC, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_IGNORE_TIMEOUT CTL_CODE(synccom_IOCTL_MAGIC, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_SET_TX_MODIFIERS CTL_CODE(synccom_IOCTL_MAGIC, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_TX_MODIFIERS CTL_CODE(synccom_IOCTL_MAGIC, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_ENABLE_RX_MULTIPLE CTL_CODE(synccom_IOCTL_MAGIC, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_DISABLE_RX_MULTIPLE CTL_CODE(synccom_IOCTL_MAGIC, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_RX_MULTIPLE CTL_CODE(synccom_IOCTL_MAGIC, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_ENABLE_APPEND_TIMESTAMP CTL_CODE(synccom_IOCTL_MAGIC, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_DISABLE_APPEND_TIMESTAMP CTL_CODE(synccom_IOCTL_MAGIC, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_APPEND_TIMESTAMP CTL_CODE(synccom_IOCTL_MAGIC, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_ENABLE_WAIT_ON_WRITE CTL_CODE(synccom_IOCTL_MAGIC, 0x816, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_DISABLE_WAIT_ON_WRITE CTL_CODE(synccom_IOCTL_MAGIC, 0x817, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_WAIT_ON_WRITE CTL_CODE(synccom_IOCTL_MAGIC, 0x818, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define synccom_TRACK_INTERRUPTS CTL_CODE(synccom_IOCTL_MAGIC, 0x819, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define synccom_GET_MEM_USAGE CTL_CODE(synccom_IOCTL_MAGIC, 0x81A, METHOD_BUFFERED, FILE_ANY_ACCESS)


enum transmit_modifiers { XF = 0, XREP = 1, TXT = 2, TXEXT = 4 };

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD  DriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD FsccDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP FsccDeviceRemove;
EVT_WDF_DEVICE_PREPARE_HARDWARE FsccDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE FsccDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY FsccDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT FsccDeviceD0Exit;
EVT_WDF_IO_QUEUE_IO_DEFAULT FsccIoDefault;
EVT_WDF_IO_QUEUE_IO_READ FsccIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE FsccIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FsccIoDeviceControl;



typedef INT64 synccom_register;

struct synccom_registers {
	/* BAR 0 */
	synccom_register __reserved1[2];

	synccom_register FIFOT;

	synccom_register __reserved2[2];

	synccom_register CMDR;
	synccom_register STAR; /* Read-only */
	synccom_register CCR0;
	synccom_register CCR1;
	synccom_register CCR2;
	synccom_register BGR;
	synccom_register SSR;
	synccom_register SMR;
	synccom_register TSR;
	synccom_register TMR;
	synccom_register RAR;
	synccom_register RAMR;
	synccom_register PPR;
	synccom_register TCR;
	synccom_register VSTR; /* Read-only */

	synccom_register __reserved4[1];

	synccom_register IMR;
	synccom_register DPLLR;

	/* BAR 2 */
	synccom_register FCR;
};

struct synccom_memory_cap {
	int input;
	int output;
};

#define synccom_REGISTERS_INIT(registers) memset(&registers, -1, sizeof(registers))
#define synccom_UPDATE_VALUE -2

#define synccom_ID 0x000f
#define Ssynccom_ID 0x0014
#define Ssynccom_104_LVDS_ID 0x0015
#define synccom_232_ID 0x0016
#define Ssynccom_104_UA_ID 0x0017
#define Ssynccom_4_UA_ID 0x0018
#define Ssynccom_UA_ID 0x0019
#define Ssynccom_LVDS_ID 0x001a
#define synccom_4_UA_ID 0x001b
#define Ssynccom_4_LVDS_ID 0x001c
#define synccom_UA_ID 0x001d
#define SFSCCe_4_ID 0x001e
#define Ssynccom_4_CPCI_ID 0x001f
#define SFSCCe_4_LVDS_UA_ID 0x0022
#define Ssynccom_4_UA_CPCI_ID 0x0023
#define Ssynccom_4_UA_LVDS_ID 0x0025
#define Ssynccom_UA_LVDS_ID 0x0026
#define FSCCe_4_UA_ID 0x0027

#define STATUS_LENGTH 2

#endif