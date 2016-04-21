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

#ifndef SYNCCOM_ISR_H
#define SYNCCOM_ISR_H

#include <ntddk.h>
#include <wdf.h>

#include "trace.h"

EVT_WDF_DPC		oframe_worker;
EVT_WDF_DPC		clear_oframe_worker;
EVT_WDF_DPC		iframe_worker;
EVT_WDF_DPC		isr_worker;
EVT_WDF_DPC		SynccomProcessRead;

int				synccom_port_frame_read(struct synccom_port *port, unsigned char *buf, size_t buf_length, size_t *out_length);
int				synccom_port_stream_read(struct synccom_port *port, unsigned char *buf, size_t buf_length, size_t *out_length);
unsigned		synccom_port_transmit_frame(struct synccom_port *port, struct synccom_frame *frame);
void			synccom_port_execute_transmit(struct synccom_port *port);
int				prepare_frame_for_fifo(struct synccom_port *port, struct synccom_frame *frame, unsigned *length);
NTSTATUS		synccom_port_data_write(struct synccom_port *port, const unsigned char *data, unsigned byte_count);
void			register_completion(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context);
void			basic_completion(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context);
synccom_register	synccom_port_read_data_async(struct synccom_port *port, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context);

EVT_WDF_IO_QUEUE_IO_STOP SyncComEvtIoStop;
EVT_WDF_USB_READER_COMPLETION_ROUTINE port_received_data;
EVT_WDF_USB_READERS_FAILED FX3EvtReadFailed;

#endif