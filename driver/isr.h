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

#ifndef SYNCCOM_ISR_H
#define SYNCCOM_ISR_H

#include <ntddk.h>
#include <wdf.h>

#include "trace.h"

EVT_WDF_DPC		oframe_worker;
EVT_WDF_DPC		iframe_worker;
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
EVT_WDF_TIMER timer_handler;

#endif