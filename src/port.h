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

#ifndef SYNCCOM_PORT_H
#define SYNCCOM_PORT_H

#include "precomp.h"
#include "defines.h"
#include "debug.h"

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SyncComEvtIoDeviceControl;

NTSTATUS		synccom_port_set_registers(_In_ struct synccom_port *port, const struct synccom_registers *regs);
void			synccom_port_set_register_rep(_In_ struct synccom_port *port, unsigned char offset, unsigned char address, const UINT32 *buf, unsigned write_count);
NTSTATUS		synccom_port_set_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, UINT32 value, EVT_WDF_REQUEST_COMPLETION_ROUTINE write_return);

void			synccom_port_get_registers(_In_ struct synccom_port *port, struct synccom_registers *regs);
NTSTATUS        synccom_port_get_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context);
NTSTATUS        synccom_port_get_nonvolatile(struct synccom_port *port, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context);
NTSTATUS        synccom_port_set_nonvolatile(struct synccom_port *port, UINT32 value, EVT_WDF_REQUEST_COMPLETION_ROUTINE write_return);
NTSTATUS        synccom_port_get_fx2_firmware(struct synccom_port *port, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context);

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

unsigned        synccom_port_can_support_nonvolatile(struct synccom_port *port);

#endif