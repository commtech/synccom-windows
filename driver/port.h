/*
	Copyright (C) 2019  Commtech, Inc.

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

#ifndef SYNCCOM_PORT_H
#define SYNCCOM_PORT_H

#include <ntddk.h>
#include <wdf.h>
#include <defines.h>
#include "trace.h"
#include "fx2Events.h"

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SyncComEvtIoDeviceControl;

NTSTATUS		synccom_port_set_registers(_In_ struct synccom_port *port, const struct synccom_registers *regs);
void			synccom_port_set_register_rep(_In_ struct synccom_port *port, unsigned char offset, unsigned char address, const UINT32 *buf, unsigned write_count);
NTSTATUS		synccom_port_set_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, UINT32 value, EVT_WDF_REQUEST_COMPLETION_ROUTINE write_return);

void			synccom_port_get_registers(_In_ struct synccom_port *port, struct synccom_registers *regs);
synccom_register	synccom_port_get_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context);

void			synccom_port_set_clock_bits(_In_ struct synccom_port *port, unsigned char *clock_data);
NTSTATUS		synccom_port_program_firmware(_In_ struct synccom_port *port, unsigned char *firmware_line, size_t data_size);

void			synccom_port_set_memory_cap(struct synccom_port *port, struct synccom_memory_cap *memory_cap);
unsigned		synccom_port_get_input_memory_cap(struct synccom_port *port);
unsigned		synccom_port_get_output_memory_cap(struct synccom_port *port);
unsigned		synccom_port_get_output_memory_usage(struct synccom_port *port);
unsigned		synccom_port_get_input_memory_usage(struct synccom_port *port);

NTSTATUS		synccom_port_purge_tx(struct synccom_port *port);
NTSTATUS		synccom_port_purge_rx(struct synccom_port *port);

void			synccom_port_set_ignore_timeout(struct synccom_port *port, BOOLEAN ignore_timeout);
BOOLEAN			synccom_port_get_ignore_timeout(struct synccom_port *port);

void			synccom_port_set_rx_multiple(struct synccom_port *port, BOOLEAN rx_multiple);
BOOLEAN			synccom_port_get_rx_multiple(struct synccom_port *port);

NTSTATUS		synccom_port_set_tx_modifiers(struct synccom_port *port, int value);
unsigned		synccom_port_get_tx_modifiers(struct synccom_port *port);

void			synccom_port_set_append_status(struct synccom_port *port, BOOLEAN value);
BOOLEAN			synccom_port_get_append_status(struct synccom_port *port);

void			synccom_port_set_append_timestamp(struct synccom_port *port, BOOLEAN value);
BOOLEAN			synccom_port_get_append_timestamp(struct synccom_port *port);

NTSTATUS		synccom_port_execute_RRES(struct synccom_port *port);
NTSTATUS		synccom_port_execute_TRES(struct synccom_port *port);
unsigned		synccom_port_is_streaming(struct synccom_port *port);
unsigned		synccom_port_has_incoming_data(struct synccom_port *port);
UINT32			synccom_port_get_firmware_rev(struct synccom_port *port);

NTSTATUS		synccom_port_get_port_num(struct synccom_port *port, unsigned *port_num);
NTSTATUS		synccom_port_set_port_num(struct synccom_port *port, unsigned value);
#endif