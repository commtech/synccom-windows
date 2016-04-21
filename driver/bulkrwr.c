/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    bulkrwr.c

Abstract:

    This file has routines to perform reads and writes.
    The read and writes are targeted bulk to endpoints.

Environment:

    Kernel mode

--*/

#include <osrusbfx2.h>
#include <public.h>
#include <port.h>
#include <flist.h>
#include <frame.h>


#if defined(EVENT_TRACING)
#include "bulkrwr.tmh"
#endif

#pragma warning(disable:4267)

VOID SyncComEvtIoRead(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Entering.", __FUNCTION__);

	port = GetPortContext(WdfIoQueueGetDevice(Queue));

	/* The user is requsting 0 bytes so return immediately */
	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}

	status = WdfRequestForwardToIoQueue(Request, port->read_queue2);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfRequestForwardToIoQueue failed %!STATUS!", __FUNCTION__, status);
		WdfRequestComplete(Request, status);
		return;
	}

	WdfDpcEnqueue(port->process_read_dpc);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Exiting.", __FUNCTION__);
}

VOID SyncComEvtIoWrite(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
	NTSTATUS status = STATUS_SUCCESS;
	char *data_buffer = NULL;
	struct fscc_frame *frame = 0;
	struct fscc_port *port = 0;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);

	port = GetPortContext(WdfIoQueueGetDevice(Queue));
	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}

	if (fscc_port_get_output_memory_usage(port) + Length > fscc_port_get_output_memory_cap(port)) {
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	status = WdfRequestRetrieveInputBuffer(Request, Length, (PVOID *)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
		WdfRequestComplete(Request, status);
		return;
	}

	frame = fscc_frame_new(port);

	if (!frame) {
		WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	fscc_frame_add_data(frame, data_buffer, Length);
	frame->frame_size = Length;
	WdfSpinLockAcquire(port->queued_oframes_spinlock);
	fscc_flist_add_frame(&port->queued_oframes, frame);
	WdfSpinLockRelease(port->queued_oframes_spinlock);

	if (port->wait_on_write) {
		status = WdfRequestForwardToIoQueue(Request, port->write_queue2);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: WdfRequestForwardToIoQueue failed %!STATUS!", __FUNCTION__, status);
			WdfRequestComplete(Request, status);
			return;
		}
	}
	else {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
	}

	fscc_port_get_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, FIFO_BC_OFFSET, IsrCompletionRoutine, port);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.", __FUNCTION__);
}

VOID SyncComEvtIoStop(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ ULONG ActionFlags)
{
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(ActionFlags);
	if (ActionFlags &  WdfRequestStopActionSuspend) {
		WdfRequestStopAcknowledge(Request, FALSE); // Don't requeue
	}
	else if (ActionFlags &  WdfRequestStopActionPurge) {
		WdfRequestCancelSentRequest(Request);
	}
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.", __FUNCTION__);
	return;
}

int fscc_port_stream_read(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);
	return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->istream_spinlock);

	*out_length = min(buf_length, (size_t)fscc_frame_get_length(port->istream));

	fscc_frame_remove_data(port->istream, buf, (unsigned)(*out_length));

	WdfSpinLockRelease(port->istream_spinlock);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.", __FUNCTION__);
	return STATUS_SUCCESS;
}

int fscc_port_frame_read(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
	struct fscc_frame *frame = 0;
	unsigned remaining_buf_length = 0;
	int max_frame_length = 0;
	unsigned current_frame_length = 0;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);
	return_val_if_untrue(port, 0);

	*out_length = 0;

	do {
		remaining_buf_length = (unsigned)buf_length - (unsigned)*out_length;

		if (port->append_status && port->append_timestamp)
			max_frame_length = remaining_buf_length - sizeof(LARGE_INTEGER);
		else if (port->append_status)
			max_frame_length = remaining_buf_length;
		else if (port->append_timestamp)
			max_frame_length = remaining_buf_length + 2 - sizeof(LARGE_INTEGER); // Status length
		else
			max_frame_length = remaining_buf_length + 2; // Status length

		if (max_frame_length < 0)
			break;

		WdfSpinLockAcquire(port->queued_iframes_spinlock);
		frame = fscc_flist_remove_frame_if_lte(&port->queued_iframes, max_frame_length);
		WdfSpinLockRelease(port->queued_iframes_spinlock);

		if (!frame)
			break;

		current_frame_length = fscc_frame_get_length(frame);
		current_frame_length -= (port->append_status) ? 0 : 2;

		fscc_frame_remove_data(frame, buf + *out_length, current_frame_length);
		*out_length += current_frame_length;

		if (port->append_timestamp) {
			memcpy(buf + *out_length, &frame->timestamp, sizeof(frame->timestamp));
			current_frame_length += sizeof(frame->timestamp);
			*out_length += sizeof(frame->timestamp);
		}

		fscc_frame_delete(frame);
	} while (port->rx_multiple);

	if (*out_length == 0)
		return STATUS_BUFFER_TOO_SMALL;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.", __FUNCTION__);
	return STATUS_SUCCESS;
}

VOID EvtRequestWriteCompletionRoutine(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
    NTSTATUS    status;
    size_t      bytes_written = 0;
    //GUID        activity = RequestToActivityId(Request);
    PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(Request);

    status = CompletionParams->IoStatus.Status;

    //
    // For usb devices, we should look at the Usb.Completion param.
    //
    usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

    bytes_written =  usbCompletionParams->Parameters.PipeWrite.Length;

    if (NT_SUCCESS(status)){
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: Number of bytes written: %I64d\n", __FUNCTION__, (INT64)bytes_written);
    } else {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Write failed: request Status 0x%x UsbdStatus 0x%x\n", __FUNCTION__, status, usbCompletionParams->UsbdStatus);
    }

    //
    // Log write stop event, using IRP activtiy ID if available or request
    // handle otherwise
    //
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.", __FUNCTION__);
    return;
}

void fscc_port_execute_transmit(struct fscc_port *port)
{
	unsigned command_value;
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Entering.", __FUNCTION__);
	return_if_untrue(port);
	command_value = 0x01000000;
	if (port->tx_modifiers & XREP) command_value |= 0x02000000;
	if (port->tx_modifiers & TXT) command_value |= 0x10000000;
	if (port->tx_modifiers & TXEXT) command_value |= 0x2000000;
	fscc_port_set_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, CMDR_OFFSET, command_value, NULL);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Exiting.", __FUNCTION__);

}

int prepare_frame_for_fifo(struct fscc_port *port, struct fscc_frame *frame, unsigned *length)
{
	unsigned current_length = 0;
	unsigned frame_size = 0;
	unsigned fifo_space = 0;
	unsigned size_in_fifo = 0;
	unsigned transmit_length = 0;
	NTSTATUS status = STATUS_SUCCESS;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Entering.", __FUNCTION__);
	current_length = fscc_frame_get_length(frame);
	size_in_fifo = ((current_length % 4) == 0) ? current_length : current_length + (4 - current_length % 4);
	frame_size = fscc_frame_get_frame_size(frame);
	fifo_space = EXTRA_BLOCK_SIZE - 1;
	fifo_space -= fifo_space % 4;

	/* Determine the maximum amount of data we can send this time around. */
	transmit_length = (size_in_fifo > fifo_space) ? fifo_space : size_in_fifo;
	frame->fifo_initialized = 1;

	if (transmit_length == 0) return 0;

	status = fscc_port_data_write(port, frame->buffer, transmit_length);
	if (NT_SUCCESS(status)) fscc_frame_remove_data(frame, NULL, transmit_length);
	*length = transmit_length;

	/* If this is the first time we add data to the FIFO for this frame we
	tell the port how much data is in this frame. */
	if (current_length == frame_size) fscc_port_set_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, BC_FIFO_L_OFFSET, frame_size, NULL);

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Exiting.", __FUNCTION__);

	/* We still have more data to send. */
	if (!fscc_frame_is_empty(frame)) {
		return 1;
	}
	return 2;
}

NTSTATUS fscc_port_data_write(struct fscc_port *port, const char *data, unsigned byte_count)
{
	WDFMEMORY write_memory;
	WDFREQUEST write_request;
	WDF_OBJECT_ATTRIBUTES  attributes;
	NTSTATUS status = STATUS_SUCCESS;
	WDFUSBPIPE pipe;
	const char *outgoing_data = 0;
	char *reversed_data = 0;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Entering.", __FUNCTION__);
	return_val_if_untrue(port, 0);
	return_val_if_untrue(data, 0);
	return_val_if_untrue(byte_count > 0, 0);

	outgoing_data = data;
	pipe = port->data_write_pipe;

	
#ifdef __BIG_ENDIAN
	reversed_data = (char *)ExAllocatePoolWithTag(NonPagedPool, byte_count, 'ataD');

	for (i = 0; i < byte_count; i++)
		reversed_data[i] = data[byte_count - i - 1];

	outgoing_data = reversed_data;
#endif
	
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->data_write_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(pipe), &write_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	attributes.ParentObject = port->data_write_pipe;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, byte_count, &write_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	status = WdfMemoryCopyFromBuffer(write_memory, 0, (void *)outgoing_data, byte_count);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot copy buffer to memory.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	status = WdfUsbTargetPipeFormatRequestForWrite(pipe, write_request, write_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for write.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	WdfRequestSetCompletionRoutine(write_request, NULL, pipe);
	WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(write_request, WdfUsbTargetPipeGetIoTarget(pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		status = WdfRequestGetStatus(write_request);
		WdfObjectDelete(write_request);

	}
	WdfSpinLockRelease(port->request_spinlock);
	if (reversed_data)
		ExFreePoolWithTag(reversed_data, 'ataD');
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: Status %!STATUS!", __FUNCTION__, status);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Exiting.", __FUNCTION__);
	return status;
}

unsigned fscc_port_transmit_frame(struct fscc_port *port, struct fscc_frame *frame)
{
	unsigned transmit_length = 0;
	int result;

	result = prepare_frame_for_fifo(port, frame, &transmit_length);

	if (result) fscc_port_execute_transmit(port);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
		"F#%i => %i byte%s%s",
		frame->number, transmit_length,
		(transmit_length == 1) ? " " : "s",
		(result != 2) ? " (starting)" : " ");

	return result;
}
/*
NTSTATUS FX3ConfigureContinuousReader(_In_ WDFDEVICE Device,  WDFUSBPIPE Pipe)
{
	NTSTATUS status;
	struct fscc_port *port = 0;
	WDF_USB_CONTINUOUS_READER_CONFIG    readerConfig;

	PAGED_CODE();

	port = GetPortContext(Device);
	WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&readerConfig, port_received_data_reader, port, 512);
	readerConfig.EvtUsbTargetPipeReadersFailed = FX3EvtReadFailed;

	status = WdfUsbTargetPipeConfigContinuousReader( Pipe, &readerConfig);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: WdfUsbTargetPipeConfigContinuousReader failed 0x%x", __FUNCTION__, status);
		return status;
	}

	return status;
}
*/
BOOLEAN FX3EvtReadFailed(WDFUSBPIPE Pipe, NTSTATUS Status, USBD_STATUS UsbdStatus)
{
	UNREFERENCED_PARAMETER(Pipe);

	TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%!FUNC! ReadersFailedCallback failed NTSTATUS 0x%x, UsbdStatus 0x%x\n", Status, UsbdStatus);
	/*
	In the preceding example, the driver returns TRUE. This value indicates to the framework that it must reset the pipe and then restart the continuous reader.
	Alternatively, the client driver can return FALSE and provide an error recovery mechanism if a stall condition occurs on the pipe. For example, the driver can check the USBD status and issue a reset-pipe request to clear the stall condition.
	For information about error recovery in pipes, see How to recover from USB pipe errors.
	https://msdn.microsoft.com/ru-ru/library/hh968307(v=vs.85).aspx
	*/
	return TRUE;
}

void oframe_worker(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
	int result = 0;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Entering.", __FUNCTION__);
	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	return_if_untrue(port);

	WdfSpinLockAcquire(port->board_tx_spinlock);
	WdfSpinLockAcquire(port->pending_oframe_spinlock);

	/* Check if exists and if so, grabs the frame to transmit. */
	if (!port->pending_oframe) {
		WdfSpinLockAcquire(port->queued_oframes_spinlock);
		port->pending_oframe = fscc_flist_remove_frame(&port->queued_oframes);
		WdfSpinLockRelease(port->queued_oframes_spinlock);

		/* No frames in queue to transmit */
		if (!port->pending_oframe) {
			WdfSpinLockRelease(port->pending_oframe_spinlock);
			WdfSpinLockRelease(port->board_tx_spinlock);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: No queued oframes, exiting.", __FUNCTION__);
			return;
		}
	}

	result = fscc_port_transmit_frame(port, port->pending_oframe);

	if (result == 2) {
		WdfSpinLockAcquire(port->sent_oframes_spinlock);
		fscc_flist_add_frame(&port->sent_oframes, port->pending_oframe);
		WdfSpinLockRelease(port->sent_oframes_spinlock);

		port->pending_oframe = 0;
	}

	WdfSpinLockRelease(port->pending_oframe_spinlock);
	WdfSpinLockRelease(port->board_tx_spinlock);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Exiting.", __FUNCTION__);
}

void iframe_worker(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
	int result = 0;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: Entering.", __FUNCTION__);
	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	return_if_untrue(port);
	return_if_untrue(port->istream);
	
	WdfSpinLockAcquire(port->istream_spinlock);
	WdfSpinLockAcquire(port->pending_iframe_spinlock);
	if (!port->pending_iframe) {
		port->pending_iframe = fscc_frame_new(port);
		WdfSpinLockRelease(port->pending_iframe_spinlock);
		WdfSpinLockRelease(port->istream_spinlock);
		return;
	}
	if (!(port->pending_iframe->frame_size > 0)) {
		WdfSpinLockRelease(port->pending_iframe_spinlock);
		WdfSpinLockRelease(port->istream_spinlock);
		return;
	}
	result = fscc_frame_fill_frame_from_stream(port->pending_iframe, port->istream);
	if (port->pending_iframe->frame_size == port->pending_iframe->data_length) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: Finished pending_iframe!", __FUNCTION__);
		if (port->pending_iframe) {
			KeQuerySystemTime(&port->pending_iframe->timestamp);
			WdfSpinLockAcquire(port->queued_iframes_spinlock);
			fscc_flist_add_frame(&port->queued_iframes, port->pending_iframe);
			WdfSpinLockRelease(port->queued_iframes_spinlock);
		}
		port->pending_iframe = 0;
	}
	WdfSpinLockRelease(port->pending_iframe_spinlock);
	WdfSpinLockRelease(port->istream_spinlock);
	WdfDpcEnqueue(port->process_read_dpc);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: Exiting.", __FUNCTION__);
}

void isr_worker(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
	return_if_untrue(Dpc);
	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	return_if_untrue(port);
	fscc_port_get_isr(port, IsrCompletionRoutine, port);
}

void IsrCompletionRoutine(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
	NTSTATUS    status;
	size_t      bytes_read = 0;
	struct fscc_port *port = 0;
	PWDF_USB_REQUEST_COMPLETION_PARAMS usb_completion_params;
	unsigned char *read_buffer = 0, address = 0;
	WDF_REQUEST_REUSE_PARAMS  params;
	ULONG value;
	unsigned streaming = 0;

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Request);

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.\n", __FUNCTION__);
	port = (PFSCC_PORT)Context;
	return_if_untrue(port);
	usb_completion_params = CompletionParams->Parameters.Usb.Completion;
	status = CompletionParams->IoStatus.Status;

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Read failed: Request status: 0x%x, UsbdStatus: 0x%x\n", __FUNCTION__, status, usb_completion_params->UsbdStatus);
		//if (port)	 WdfRequestReuse(port->isr_read_request, &params);
		return;
	}

	bytes_read = usb_completion_params->Parameters.PipeRead.Length;
	if (bytes_read != 6) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Wrong number of bytes! Expected 6, got %d!\n", __FUNCTION__, (int)bytes_read);
		return;
	}

	read_buffer = WdfMemoryGetBuffer(usb_completion_params->Parameters.PipeRead.Buffer, NULL);
	address = read_buffer[1] >> 1;
	memcpy(&value, &read_buffer[bytes_read - 4], 4);

#ifdef __BIG_ENDIAN
#else
	value = _byteswap_ulong(value);
#endif

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: Updating register storage: 0x%2.2X%2.2X 0x%8.8X -> 0x%2.2X%2.2X 0x%8.8X.\n", __FUNCTION__, read_buffer[0], address, (unsigned int)((fscc_register *)&port->register_storage)[(address / 4)], read_buffer[0], address, (UINT32)value);
	WdfSpinLockAcquire(port->board_settings_spinlock);
	if ((address < MAX_OFFSET) && (read_buffer[0] == 0x80)) ((fscc_register *)&port->register_storage)[(address / 4)] = value;
	if ((address == FCR_OFFSET) && (read_buffer[0] == 0x00)) port->register_storage.FCR = value;
	WdfSpinLockRelease(port->board_settings_spinlock);
	
	switch (address)
	{
	case ISR_OFFSET:
		port->last_isr_value |= (UINT32)value;
		streaming = fscc_port_is_streaming(port);
		if (value & TFT) {
			WdfDpcEnqueue(port->oframe_dpc);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: TFT.\n", __FUNCTION__);
		}
		if (value & RFE) {
			fscc_port_get_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, FIFO_BC_OFFSET, IsrCompletionRoutine, port);
			fscc_port_get_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, BC_FIFO_L_OFFSET, IsrCompletionRoutine, port);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: RFE.\n", __FUNCTION__);
		}
		if (value & RFS) {
			fscc_port_get_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, FIFO_BC_OFFSET, IsrCompletionRoutine, port);
			fscc_port_set_register_async(port, FPGA_UPPER_ADDRESS, CONTROL_OFFSET, FLUSH_COMMAND, NULL);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: RFS.\n", __FUNCTION__);
		}
		if (value & RFT) {
			fscc_port_get_register_async(port, FPGA_UPPER_ADDRESS + FSCC_UPPER_OFFSET, FIFO_BC_OFFSET, IsrCompletionRoutine, port);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: RFT.\n", __FUNCTION__);
		}
		WdfRequestReuse(port->isr_read_request, &params);
		WdfDpcEnqueue(port->isr_dpc);
		break;
	case FIFO_FC_OFFSET:
		break;
	case BC_FIFO_L_OFFSET:
		WdfSpinLockAcquire(port->istream_spinlock);
		WdfSpinLockAcquire(port->pending_iframe_spinlock);
		if (!port->pending_iframe) {
			port->pending_iframe = fscc_frame_new(port);
			if (!port->pending_iframe) {
				WdfSpinLockRelease(port->pending_iframe_spinlock);
				WdfSpinLockRelease(port->board_rx_spinlock);
				break;
			}
		}
		if (port->pending_iframe->frame_size == 0) {
			port->pending_iframe->frame_size = value;
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: frame_size is now %d.", __FUNCTION__, port->pending_iframe->frame_size);
		}
		else TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: frame_size is already set to %d, not %d.", __FUNCTION__, port->pending_iframe->frame_size, value);
		WdfSpinLockRelease(port->pending_iframe_spinlock);
		WdfSpinLockRelease(port->istream_spinlock);
		WdfDpcEnqueue(port->iframe_dpc);
		break;
	case STAR_OFFSET:
		// Do we | or do we &? Or a weird combo of both?
		// Requeue STAR.
		break;
	case FIFO_BC_OFFSET:
		if (((value & 0x1FFF0000) >> 16) < ((port->register_storage.FIFOT & 0x0fff0000) >> 16)) WdfDpcEnqueue(port->oframe_dpc);
		if ((value & 0x3FFF) > (port->register_storage.FIFOT & 0x0000ffff)) {
			fscc_port_set_register_async(port, FPGA_UPPER_ADDRESS, CONTROL_OFFSET, FLUSH_COMMAND, NULL);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
			read_data_async(port, port_received_data, port);
		}
		break;
	}
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Exiting.\n", __FUNCTION__);
	return;
}

void port_received_data(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
	struct fscc_port *port = 0;
	unsigned current_memory = 0;
	unsigned memory_cap = 0;
	size_t receive_length;
	NTSTATUS status = STATUS_SUCCESS;
	static int rejected_last_stream = 0;
	char *read_buffer = 0;
	PWDF_USB_REQUEST_COMPLETION_PARAMS usb_completion_params;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_WRITE, "%s: Entering.", __FUNCTION__);
	UNREFERENCED_PARAMETER(Request);
	UNREFERENCED_PARAMETER(Target);

	port = (PFSCC_PORT)Context;
	usb_completion_params = CompletionParams->Parameters.Usb.Completion;
	status = CompletionParams->IoStatus.Status;
	return_if_untrue(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Read failed: Request status: 0x%x", __FUNCTION__, status);
		return;
	}
	receive_length = usb_completion_params->Parameters.PipeRead.Length;
	return_if_untrue(receive_length > 0);
	read_buffer = WdfMemoryGetBuffer(usb_completion_params->Parameters.PipeRead.Buffer, NULL);

	current_memory = fscc_port_get_input_memory_usage(port);
	memory_cap = fscc_port_get_input_memory_cap(port);
	if (current_memory >= memory_cap)
	{
		if (rejected_last_stream == 0) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Rejecting data (memory constraint), lost %d bytes.", __FUNCTION__, receive_length);
			rejected_last_stream = 1;
		}
		return;
	}
	if (receive_length + current_memory > memory_cap) receive_length = memory_cap - current_memory;
	WdfSpinLockAcquire(port->istream_spinlock);
	status = fscc_frame_add_data(port->istream, read_buffer, (int)receive_length);
	WdfSpinLockRelease(port->istream_spinlock);

	if (status == FALSE) TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Failed to add data to istream.", __FUNCTION__);
	else TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: Stream <= %i byte%s", __FUNCTION__, (int)receive_length, (receive_length == 1) ? "" : "s");
	rejected_last_stream = 0;
	WdfDpcEnqueue(port->iframe_dpc);
	return;
}