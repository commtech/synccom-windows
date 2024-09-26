/*
Copyright 2023 Commtech, Inc.

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
#include "port.h"
#include "flist.h"
#include "frame.h"
#include "isr.h"


#if defined(EVENT_TRACING)
#include "isr.tmh"
#endif

#pragma warning(disable:4267)

VOID SyncComEvtIoRead(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct synccom_port* port = 0;

	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}
	port = GetPortContext(WdfIoQueueGetDevice(Queue));
	if (!port) {
		WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		return;
	}
	status = WdfRequestForwardToIoQueue(Request, port->read_queue2);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfRequestForwardToIoQueue failed %!STATUS!", __FUNCTION__, status);
		WdfRequestComplete(Request, status);
		return;
	}
	WdfDpcEnqueue(port->process_read_dpc);
}

VOID SyncComEvtIoWrite(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
	NTSTATUS status = STATUS_SUCCESS;
	unsigned char* data_buffer = NULL;
	struct synccom_frame* frame = 0;
	struct synccom_port* port = 0;

	port = GetPortContext(WdfIoQueueGetDevice(Queue));
	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}

	if (synccom_port_get_output_memory_usage(port) + Length > synccom_port_get_output_memory_cap(port)) {
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}

	status = WdfRequestRetrieveInputBuffer(Request, Length, (PVOID*)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
		WdfRequestComplete(Request, status);
		return;
	}

	frame = synccom_frame_new(port);

	if (!frame) {
		WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	synccom_frame_add_data(frame, data_buffer, Length);
	frame->frame_size = Length;
	WdfSpinLockAcquire(port->queued_oframes_spinlock);
	synccom_flist_add_frame(&port->queued_oframes, frame);
	WdfSpinLockRelease(port->queued_oframes_spinlock);

	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
	WdfDpcEnqueue(port->oframe_dpc);
}

VOID SyncComEvtIoStop(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ ULONG ActionFlags)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(ActionFlags);
	if (ActionFlags & WdfRequestStopActionSuspend) {
		WdfRequestStopAcknowledge(Request, FALSE); // Don't requeue
	}
	else if (ActionFlags & WdfRequestStopActionPurge) {
		WdfRequestCancelSentRequest(Request);
	}
	return;
}

void SynccomProcessRead(WDFDPC Dpc)
{
	struct synccom_port* port = 0;
	NTSTATUS status = STATUS_SUCCESS;
	unsigned char* data_buffer = NULL;
	size_t read_count = 0;
	WDFREQUEST request;
	unsigned length = 0;
	WDF_REQUEST_PARAMETERS params;

	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	if (!synccom_port_has_incoming_data(port)) return;
	status = WdfIoQueueRetrieveNextRequest(port->read_queue2, &request);
	if (!NT_SUCCESS(status)) {
		if (status != STATUS_NO_MORE_ENTRIES) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueRetrieveNextRequest failed %!STATUS!", status);
		}

		return;
	}
	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(request, &params);
	length = (unsigned)params.Parameters.Read.Length;

	status = WdfRequestRetrieveOutputBuffer(request, length, (PVOID*)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
		WdfRequestComplete(request, status);
		return;
	}

	if (synccom_port_is_streaming(port))
		status = synccom_port_stream_read(port, data_buffer, length, &read_count);
	else
		status = synccom_port_frame_read(port, data_buffer, length, &read_count);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_PNP, "synccom_port_{frame,stream}_read failed %!STATUS!", status);
		WdfRequestComplete(request, status);
		return;
	}

	WdfRequestCompleteWithInformation(request, status, read_count);
}

int synccom_port_stream_read(struct synccom_port* port, unsigned char* buf, size_t buf_length, size_t* out_length)
{
	return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->istream_spinlock);
	*out_length = min(buf_length, (size_t)synccom_frame_get_length(port->istream));
	synccom_frame_remove_data(port->istream, buf, (unsigned)(*out_length));
	WdfSpinLockRelease(port->istream_spinlock);

	return STATUS_SUCCESS;
}

int synccom_port_frame_read(struct synccom_port* port, unsigned char* buf, size_t buf_length, size_t* out_length)
{
	struct synccom_frame* frame = 0;
	unsigned remaining_buf_length = 0;
	int max_frame_length = 0;
	unsigned current_frame_length = 0;

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
		frame = synccom_flist_remove_frame_if_lte(&port->queued_iframes, max_frame_length);
		WdfSpinLockRelease(port->queued_iframes_spinlock);

		if (!frame)
			break;

		current_frame_length = synccom_frame_get_length(frame);
		current_frame_length -= (port->append_status) ? 0 : 2;

		synccom_frame_remove_data(frame, buf + *out_length, current_frame_length);
		*out_length += current_frame_length;

		if (port->append_timestamp) {
			memcpy(buf + *out_length, &frame->timestamp, sizeof(frame->timestamp));
			current_frame_length += sizeof(frame->timestamp);
			*out_length += sizeof(frame->timestamp);
		}

		synccom_frame_delete(frame);
	} while (port->rx_multiple);

	if (*out_length == 0)
		return STATUS_BUFFER_TOO_SMALL;

	return STATUS_SUCCESS;
}

void synccom_port_execute_transmit(struct synccom_port* port)
{
	unsigned command_value;

	return_if_untrue(port);
	command_value = 0x01000000;
	if (port->tx_modifiers & XREP) command_value |= 0x02000000;
	if (port->tx_modifiers & TXT) command_value |= 0x10000000;
	if (port->tx_modifiers & TXEXT) command_value |= 0x20000000;
	synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, CMDR_OFFSET, command_value, basic_completion);
}

int prepare_frame_for_fifo(struct synccom_port* port, struct synccom_frame* frame, unsigned* length)
{
	unsigned current_length = 0;
	unsigned frame_size = 0;
	unsigned fifo_space = 0;
	unsigned size_in_fifo = 0;
	unsigned transmit_length = 0;
	NTSTATUS status = STATUS_SUCCESS;

	current_length = synccom_frame_get_length(frame);
	size_in_fifo = ((current_length % 4) == 0) ? current_length : current_length + (4 - current_length % 4);
	frame_size = synccom_frame_get_frame_size(frame);
	fifo_space = TX_FIFO_SIZE - 1;
	fifo_space -= fifo_space % 4;

	/* Determine the maximum amount of data we can send this time around. */
	transmit_length = (size_in_fifo > fifo_space) ? fifo_space : size_in_fifo;

	if (transmit_length < 1) return 0;

	status = synccom_port_data_write(port, frame->buffer, transmit_length);
	if (NT_SUCCESS(status)) synccom_frame_remove_data(frame, NULL, transmit_length);
	*length = transmit_length;

	/* If this is the first time we add data to the FIFO for this frame we tell the port how much data is in this frame. */
	if (current_length == frame_size) synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, BC_FIFO_L_OFFSET, frame_size, basic_completion);
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: setting oframe_size: %d.", __FUNCTION__, frame_size);
	synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, FIFO_BC_OFFSET, register_completion, port);

	/* We still have more data to send. */
	if (!synccom_frame_is_empty(frame)) {
		return 1;
	}
	return 2;
}

NTSTATUS synccom_port_data_write(struct synccom_port* port, const unsigned char* data, unsigned byte_count)
{
	WDFMEMORY write_memory;
	WDFREQUEST write_request;
	WDF_OBJECT_ATTRIBUTES  attributes;
	NTSTATUS status = STATUS_SUCCESS;
	WDFUSBPIPE pipe;
	const unsigned char* outgoing_data = 0;
	unsigned char* reversed_data = 0;

	return_val_if_untrue(port, 0);
	return_val_if_untrue(data, 0);
	return_val_if_untrue(byte_count > 0, 0);

	outgoing_data = data;
	pipe = port->data_write_pipe;


#ifdef __BIG_ENDIAN
	reversed_data = (unsigned char*)ExAllocatePoolWithTag(NonPagedPool, byte_count, 'ataD');

	for (i = 0; i < byte_count; i++)
		reversed_data[i] = data[byte_count - i - 1];

	outgoing_data = reversed_data;
#endif

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->data_write_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(pipe), &write_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		return status;
	}
	attributes.ParentObject = write_request;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, byte_count, &write_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	status = WdfMemoryCopyFromBuffer(write_memory, 0, (void*)outgoing_data, byte_count);
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
	WdfRequestSetCompletionRoutine(write_request, tx_completion, port);
	//WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(write_request, WdfUsbTargetPipeGetIoTarget(pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		status = WdfRequestGetStatus(write_request);
		WdfObjectDelete(write_request);
	}
	port->writes_in_flight++;
	//WdfSpinLockRelease(port->request_spinlock);

	if (reversed_data)
		ExFreePoolWithTag(reversed_data, 'ataD');

	return status;
}

unsigned synccom_port_transmit_frame(struct synccom_port* port, struct synccom_frame* frame)
{
	unsigned transmit_length = 0;
	int result;

	result = prepare_frame_for_fifo(port, frame, &transmit_length);

	if (result) {
		synccom_port_execute_transmit(port);
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP,
			"F#%i => %i byte%s%s",
			frame->number, frame->frame_size,
			(frame->frame_size == 1) ? " " : "s",
			(result != 2) ? " (starting)" : " ");
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: New outgoing frame: %d bytes!\n", __FUNCTION__, frame->frame_size);
	return result;
}

void oframe_worker(WDFDPC Dpc)
{
	struct synccom_port* port = 0;
	int result = 0;

	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	return_if_untrue(port);
	if (port->writes_in_flight > MAXIMUM_WRITES_IN_FLIGHT) return;

	WdfSpinLockAcquire(port->board_tx_spinlock);
	WdfSpinLockAcquire(port->pending_oframe_spinlock);

	/* Check if exists and if so, grabs the frame to transmit. */
	if (!port->pending_oframe) {
		WdfSpinLockAcquire(port->queued_oframes_spinlock);
		port->pending_oframe = synccom_flist_remove_frame(&port->queued_oframes);
		WdfSpinLockRelease(port->queued_oframes_spinlock);

		/* No frames in queue to transmit */
		if (!port->pending_oframe) {
			WdfSpinLockRelease(port->pending_oframe_spinlock);
			WdfSpinLockRelease(port->board_tx_spinlock);
			TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: No queued oframes, exiting.", __FUNCTION__);
			return;
		}
	}

	result = synccom_port_transmit_frame(port, port->pending_oframe);

	if (result == 2) {
		synccom_frame_delete(port->pending_oframe);
		port->pending_oframe = 0;
	}
	WdfSpinLockRelease(port->pending_oframe_spinlock);
	WdfSpinLockRelease(port->board_tx_spinlock);
	if (result != 0) {
		WdfDpcEnqueue(port->oframe_dpc);
	}
}

void iframe_worker(WDFDPC Dpc) {
	struct synccom_port* port = 0;
	struct synccom_frame* frame = 0;
	int result = 0, do_true = 1;

	port = GetPortContext(WdfDpcGetParentObject(Dpc));
	return_if_untrue(port);
	return_if_untrue(port->istream);


	WdfSpinLockAcquire(port->istream_spinlock);
	WdfSpinLockAcquire(port->pending_iframes_spinlock);
	do {
		if (!port->istream) break;
		if (!(port->istream->data_length > 0)) break;
		if (synccom_flist_is_empty(&port->pending_iframes)) break;
		frame = synccom_flist_peek_front(&port->pending_iframes);
		if (!frame) break;
		if (!(frame->frame_size > 0)) {
			frame = synccom_flist_remove_frame(&port->pending_iframes);
			synccom_frame_delete(frame);
			continue;
		}
		result = synccom_frame_copy_data(frame, port->istream);
		if (frame->data_length + frame->lost_bytes >= frame->frame_size)
		{
			struct synccom_frame* finished_frame = 0;
			finished_frame = synccom_frame_new(port);
			finished_frame->frame_size = synccom_frame_get_frame_size(frame);
			synccom_frame_copy_data(finished_frame, frame);
			KeQuerySystemTime(&finished_frame->timestamp);
			TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "F#%i <= %i byte%s", finished_frame->number, finished_frame->frame_size, (finished_frame->frame_size == 1) ? " " : "s");
			WdfSpinLockAcquire(port->queued_iframes_spinlock);
			synccom_flist_add_frame(&port->queued_iframes, finished_frame);
			WdfSpinLockRelease(port->queued_iframes_spinlock);

			frame = synccom_flist_remove_frame(&port->pending_iframes);
			synccom_frame_delete(frame);
		}
	} while (do_true);
	WdfSpinLockRelease(port->pending_iframes_spinlock);
	WdfSpinLockRelease(port->istream_spinlock);

	WdfDpcEnqueue(port->process_read_dpc);
}

void register_completion(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
	NTSTATUS    status;
	size_t      bytes_read = 0;
	struct synccom_port* port = 0;
	PWDF_USB_REQUEST_COMPLETION_PARAMS usb_completion_params;
	unsigned char* read_buffer = 0, address = 0;
	ULONG value = 0;

	UNREFERENCED_PARAMETER(Target);

	port = (PSYNCCOM_PORT)Context;
	if (!port) {
		WdfObjectDelete(Request);
		return;
	}
	usb_completion_params = CompletionParams->Parameters.Usb.Completion;
	status = CompletionParams->IoStatus.Status;

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Read failed: Request status: 0x%x, UsbdStatus: 0x%x\n", __FUNCTION__, status, usb_completion_params->UsbdStatus);
		WdfObjectDelete(Request);
		return;
	}

	bytes_read = usb_completion_params->Parameters.PipeRead.Length;
	read_buffer = WdfMemoryGetBuffer(usb_completion_params->Parameters.PipeRead.Buffer, NULL);
	if (bytes_read > 3) memcpy(&value, &read_buffer[bytes_read - 4], 4);
#ifdef __BIG_ENDIAN
#else
	value = _byteswap_ulong(value);
#endif
	if (bytes_read != 6) {
		if (bytes_read == 4) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: New non-volatile register value: 0x%8.8x\n", __FUNCTION__, value);
			port->nonvolatile_reg = value;
		}
		if (bytes_read == 2) {
			port->fx2_firmware_rev = read_buffer[0];
			port->fx2_firmware_rev = (port->fx2_firmware_rev << 8) | read_buffer[1];
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: New firmware register read: 0x%8.8x", __FUNCTION__, port->fx2_firmware_rev);
		}
		WdfObjectDelete(Request);
		return;
	}

	address = read_buffer[1] >> 1;

	if (address == 0x00) port->fpga_firmware_rev = value;
	if ((address == 0x00) && (read_buffer[0] == 0x00)) TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Sync Com firmware: 0x%8.8x\n", __FUNCTION__, value);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: Read register: 0x%2.2X%2.2X 0x%8.8X -> 0x%2.2X%2.2X 0x%8.8X.\n", __FUNCTION__, read_buffer[0], address, (unsigned int)((synccom_register*)&port->register_storage)[(address / 4)], read_buffer[0], address, (UINT32)value);
	WdfSpinLockAcquire(port->board_settings_spinlock);
	if ((address < MAX_OFFSET) && (read_buffer[0] == 0x80)) ((synccom_register*)&port->register_storage)[(address / 4)] = value;
	if ((address == FCR_OFFSET) && (read_buffer[0] == 0x00)) port->register_storage.FCR = value;
	WdfSpinLockRelease(port->board_settings_spinlock);

	switch (address)
	{
	case FIFO_FC_OFFSET:
		if (value & 0x3ff)
		{
			int remaining_frames, i;
			// Race conditions - Here we get the number of valid frame sizes we have
			// and we compare it to how many pending 'frame size reads' we have.
			// If there are more frame sizes, we read that many. Otherwise we move on.
			// This is because if you over-read the 'frame size' register, you get 
			// invalid and dummy data, which can throw all of our processing off.
			WdfSpinLockAcquire(port->frame_size_spinlock);
			remaining_frames = (value & 0x3ff) - port->pending_frame_size_reads;
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: %d pending frame sizes, pending %d frame size reads, %d new frame size reads\n", __FUNCTION__, value & 0x3ff, port->pending_frame_size_reads, remaining_frames);
			for (i = 0; i < remaining_frames; i++)
			{
				synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, BC_FIFO_L_OFFSET, register_completion, port);
				port->pending_frame_size_reads++;
			}
			WdfSpinLockRelease(port->frame_size_spinlock);
		}
		break;
	case BC_FIFO_L_OFFSET:
		// Race conditions - we need to make sure we haven't gotten bad data
		// from over-reading the 'frame size' register. To do this we keep track
		// of how many valid frames there are and how many pending reads to the 
		// 'frame size' register we've sent. If the number of reads exceeds the
		// valid number of frames, we should discard that data.
		WdfSpinLockAcquire(port->frame_size_spinlock);
		int current_read_value = port->pending_frame_size_reads;
		if (port->pending_frame_size_reads > 0) port->pending_frame_size_reads--;
		WdfSpinLockRelease(port->frame_size_spinlock);

		if (value > 0 && current_read_value > 0)
		{
			struct synccom_frame* frame = 0;
			frame = synccom_frame_new(port);
			frame->frame_size = value;
			WdfSpinLockAcquire(port->pending_iframes_spinlock);
			synccom_flist_add_frame(&port->pending_iframes, frame);
			WdfSpinLockRelease(port->pending_iframes_spinlock);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: New incoming frame: %d bytes!\n", __FUNCTION__, value);
		}
		synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, FIFO_FC_OFFSET, register_completion, port);
		WdfDpcEnqueue(port->iframe_dpc);
		break;
	case STAR_OFFSET:
		break;
	case FIFO_BC_OFFSET:
		WdfDpcEnqueue(port->oframe_dpc);
		break;
	}
	WdfObjectDelete(Request);
	return;
}

void basic_completion(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
	PWDF_USB_REQUEST_COMPLETION_PARAMS usb_completion_params;
	NTSTATUS    status;

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Context);

	usb_completion_params = CompletionParams->Parameters.Usb.Completion;
	status = CompletionParams->IoStatus.Status;

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Read failed: Request status: 0x%x, UsbdStatus: 0x%x\n", __FUNCTION__, status, usb_completion_params->UsbdStatus);
	}
	WdfObjectDelete(Request);
	return;
}

void tx_completion(_In_ WDFREQUEST Request, _In_ WDFIOTARGET Target, _In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, _In_ WDFCONTEXT Context)
{
	struct synccom_port* port = 0;

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(CompletionParams);

	port = (PSYNCCOM_PORT)Context;
	if (!port) {
		WdfObjectDelete(Request);
		return;
	}

	port->writes_in_flight--;
	
	WdfObjectDelete(Request);
	return;
}

void port_received_data(__in  WDFUSBPIPE Pipe, __in  WDFMEMORY Buffer, __in  size_t NumBytesTransferred, __in  WDFCONTEXT Context)
{
	struct synccom_port* port = 0;
	NTSTATUS status = STATUS_SUCCESS;
	unsigned char* read_buffer = 0;
	unsigned current_memory = 0, memory_cap = 0;
	static int rejected_last_stream = 0;
	size_t payload_size = 0, receive_length = 0, buffer_size = 0;

	UNREFERENCED_PARAMETER(Pipe);

	port = (PSYNCCOM_PORT)Context;
	return_if_untrue(port);
	return_if_untrue(NumBytesTransferred > 2);

	receive_length = NumBytesTransferred;

	if (receive_length % 2) TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: receive_length not even: %d.", __FUNCTION__, receive_length);
	read_buffer = WdfMemoryGetBuffer(Buffer, &buffer_size);

	payload_size = (size_t)read_buffer[0];
	payload_size = (payload_size << 8) | (size_t)read_buffer[1];
	if (payload_size > receive_length - 2)
	{
		TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Payload larger than buffer-2: Payload: %d, Receive Length: %d.", __FUNCTION__, payload_size, receive_length);
		return;
	}

#ifdef __BIG_ENDIAN
#else
	{
		unsigned i = 0;
		unsigned char storage;

		for (i = 0; i < receive_length; i = i + 2) {
			storage = read_buffer[i];
			read_buffer[i] = read_buffer[i + 1];
			read_buffer[i + 1] = storage;
		}
	}
#endif

	synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, FIFO_FC_OFFSET, register_completion, port);

	current_memory = synccom_port_get_input_memory_usage(port);
	memory_cap = synccom_port_get_input_memory_cap(port);
	if (payload_size + current_memory > memory_cap)
	{
		unsigned bytes_lost = payload_size + current_memory - memory_cap;
		struct synccom_frame* frame = 0;

		if (bytes_lost > payload_size) bytes_lost = payload_size;
		payload_size = memory_cap - current_memory;
		if (!synccom_port_is_streaming(port))
		{
			frame = synccom_flist_peek_back(&port->pending_iframes);
			if (frame) frame->lost_bytes += bytes_lost;
		}
		TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Payload too large, new payload size: %d.", __FUNCTION__, payload_size);
		if (payload_size < 1) return;
	}

	WdfSpinLockAcquire(port->istream_spinlock);
	status = synccom_frame_add_data(port->istream, read_buffer + 2, (unsigned)payload_size);
	WdfSpinLockRelease(port->istream_spinlock);

	if (status == FALSE) TraceEvents(TRACE_LEVEL_WARNING, DBG_WRITE, "%s: Failed to add data to istream.", __FUNCTION__);
	else TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "%s: Stream <= %i byte%s", __FUNCTION__, (int)payload_size, (payload_size == 1) ? " " : "s");
	rejected_last_stream = 0;
	WdfDpcEnqueue(port->iframe_dpc);
	return;
}

BOOLEAN FX3EvtReadFailed(WDFUSBPIPE Pipe, NTSTATUS Status, USBD_STATUS UsbdStatus)
{
	UNREFERENCED_PARAMETER(Pipe);

	TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE, "%s: Data ContinuousReader failed - did you unplug? STATUS: 0x%x, USBD_STATUS: 0x%x", __FUNCTION__, Status, UsbdStatus);
	return TRUE;
}

VOID timer_handler(WDFTIMER Timer)
{
	struct synccom_port* port = 0;

	port = GetPortContext(WdfTimerGetParentObject(Timer));
	return_if_untrue(port);

	WdfDpcEnqueue(port->oframe_dpc);
	WdfDpcEnqueue(port->iframe_dpc);
}