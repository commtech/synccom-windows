/*
	Copyright (C) 2018  Commtech, Inc.

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

#include <driver.h>
#include <port.h>
#include <utils.h>
#include <flist.h>
#include <frame.h>
#include <isr.h>

#if defined(EVENT_TRACING)
#include "port.tmh"
#endif

#pragma alloc_text(PAGE, SyncComEvtIoDeviceControl)

VOID SyncComEvtIoDeviceControl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ ULONG IoControlCode)
{
    WDFDEVICE           device;
    BOOLEAN             request_pending = FALSE;
	NTSTATUS            status = STATUS_INVALID_DEVICE_REQUEST; 
	struct synccom_port	*port = 0;
	size_t				bytes_returned = 0;

    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    PAGED_CODE();

    device = WdfIoQueueGetDevice(Queue);
	port = GetPortContext(device);

	switch (IoControlCode) {

	case SYNCCOM_SET_REGISTERS: {
		struct synccom_registers *input_regs = 0;

		status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input_regs), (PVOID *)&input_regs, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}
		WdfSpinLockAcquire(port->board_settings_spinlock);
		status = synccom_port_set_registers(port, input_regs);
		WdfSpinLockRelease(port->board_settings_spinlock);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: synccom_port_set_registers failed %!STATUS!", __FUNCTION__, status);
			break;
		}
		bytes_returned = 0;
	}
		break;
	case SYNCCOM_GET_REGISTERS: {
		struct synccom_registers *input_regs = 0;
		struct synccom_registers *output_regs = 0;

		status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input_regs), (PVOID *)&input_regs, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}
		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*output_regs), (PVOID *)&output_regs, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}
		memcpy(output_regs, input_regs, sizeof(struct synccom_registers));

		synccom_port_get_registers(port, output_regs);

		bytes_returned = sizeof(*output_regs);
	}
		break;
	case SYNCCOM_PURGE_TX: {
		status = synccom_port_purge_tx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: synccom_port_purge_tx failed %!STATUS!", __FUNCTION__, status);
			break;
		}
	}
		break;
	case SYNCCOM_PURGE_RX: {
		status = synccom_port_purge_rx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: synccom_port_purge_rx failed %!STATUS!", __FUNCTION__, status);
			break;
		}
	}
		break;
	case SYNCCOM_ENABLE_APPEND_STATUS:
		synccom_port_set_append_status(port, TRUE);
		break;
	case SYNCCOM_DISABLE_APPEND_STATUS:
		synccom_port_set_append_status(port, FALSE);
		break;
	case SYNCCOM_GET_APPEND_STATUS: {
		BOOLEAN *append_status = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*append_status), (PVOID *)&append_status, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		*append_status = synccom_port_get_append_status(port);
		bytes_returned = sizeof(*append_status);
	}
		break;
	case SYNCCOM_ENABLE_APPEND_TIMESTAMP:
		synccom_port_set_append_timestamp(port, TRUE);
		break;
	case SYNCCOM_DISABLE_APPEND_TIMESTAMP:
		synccom_port_set_append_timestamp(port, FALSE);
		break;
	case SYNCCOM_GET_APPEND_TIMESTAMP: {
		BOOLEAN *append_timestamp = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*append_timestamp), (PVOID *)&append_timestamp, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		*append_timestamp = synccom_port_get_append_timestamp(port);
		bytes_returned = sizeof(*append_timestamp);
	}
		break;
	case SYNCCOM_ENABLE_RX_MULTIPLE:
		synccom_port_set_rx_multiple(port, TRUE);
		break;
	case SYNCCOM_DISABLE_RX_MULTIPLE:
		synccom_port_set_rx_multiple(port, FALSE);
		break;
	case SYNCCOM_GET_RX_MULTIPLE: {
		BOOLEAN *rx_multiple = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*rx_multiple), (PVOID *)&rx_multiple, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		*rx_multiple = synccom_port_get_rx_multiple(port);
		bytes_returned = sizeof(*rx_multiple);
	}
		break;
	case SYNCCOM_SET_MEMORY_CAP: {
		struct synccom_memory_cap *memcap = 0;

		status = WdfRequestRetrieveInputBuffer(Request, sizeof(*memcap), (PVOID *)&memcap, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		synccom_port_set_memory_cap(port, memcap);
	}
		break;
	case SYNCCOM_GET_MEMORY_CAP: {
		struct synccom_memory_cap *memcap = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*memcap), (PVOID *)&memcap, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		memcap->input = synccom_port_get_input_memory_cap(port);
		memcap->output = synccom_port_get_output_memory_cap(port);
		bytes_returned = sizeof(*memcap);
	}
		break;
	case SYNCCOM_ENABLE_IGNORE_TIMEOUT:
		synccom_port_set_ignore_timeout(port, TRUE);
		break;
	case SYNCCOM_DISABLE_IGNORE_TIMEOUT:
		synccom_port_set_ignore_timeout(port, FALSE);
		break;
	case SYNCCOM_GET_IGNORE_TIMEOUT: {
		BOOLEAN *ignore_timeout = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*ignore_timeout), (PVOID *)&ignore_timeout, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		*ignore_timeout = synccom_port_get_ignore_timeout(port);
		bytes_returned = sizeof(*ignore_timeout);
	}
		break;
	case SYNCCOM_SET_TX_MODIFIERS: {
		int *tx_modifiers = 0;

		status = WdfRequestRetrieveInputBuffer(Request, sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		status = synccom_port_set_tx_modifiers(port, *tx_modifiers);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: synccom_port_set_tx_modifiers failed %!STATUS!", __FUNCTION__, status);
			break;
		}
	}
		break;
	case SYNCCOM_GET_TX_MODIFIERS: {
		unsigned *tx_modifiers = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveOutputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		*tx_modifiers = synccom_port_get_tx_modifiers(port);
		bytes_returned = sizeof(*tx_modifiers);
	}
		break;
	case SYNCCOM_GET_WAIT_ON_WRITE: {
		status = STATUS_NOT_SUPPORTED;
		break;
	case SYNCCOM_ENABLE_WAIT_ON_WRITE:
		status = STATUS_NOT_SUPPORTED;
		break;
	case SYNCCOM_DISABLE_WAIT_ON_WRITE:
		status = STATUS_NOT_SUPPORTED;
		break;
	case SYNCCOM_GET_MEM_USAGE: {
		struct synccom_memory_cap *memcap = 0;

		status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*memcap), (PVOID *)&memcap, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
			break;
		}

		memcap->input = synccom_port_get_input_memory_usage(port);
		memcap->output = synccom_port_get_output_memory_usage(port);

		bytes_returned = sizeof(*memcap);
	}
		break;
	case SYNCCOM_SET_CLOCK_BITS: {
		unsigned char *clock_bits = 0;

		status = WdfRequestRetrieveInputBuffer(Request, NUM_CLOCK_BYTES, (PVOID *)&clock_bits, NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
			break;
		}
		synccom_port_set_clock_bits(port, clock_bits);
	}
		break;
	case SYNCCOM_REPROGRAM: {
		unsigned char *firmware_line = 0;
		size_t buffer_size = 0, data_size = 0;
		status = WdfRequestRetrieveInputBuffer(Request, 1, &firmware_line, &buffer_size);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "WdfRequestRetrieveInputMemory failed %!STATUS!", status);
			break;
		}
		// Best safety check I can think of - highest should be ~47, so this will see a runaway line.
		for (data_size = 0; data_size < buffer_size; data_size++) {
			if (firmware_line[data_size] == '\n') break;
			if (data_size > 50) break;
		}

		if (data_size < 51) status = synccom_port_program_firmware(port, firmware_line, data_size);
		// Currently I get, store, get, modify, send the buffer - there's a better way, I'm just rushed.
	}
		break;
	case SYNCCOM_GET_FIRMWARE: {
			UINT32 *firmware_revision = 0;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*firmware_revision), (PVOID *)&firmware_revision, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestRetrieveInputBuffer failed %!STATUS!", __FUNCTION__, status);
				break;
			}
			*firmware_revision = synccom_port_get_firmware_rev(port);
			bytes_returned = sizeof(*firmware_revision);
		}
		break;
	default: {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: Default?\n", __FUNCTION__);
		status = STATUS_INVALID_DEVICE_REQUEST;
	}
		break;
	}
    if (request_pending == FALSE) {
		WdfRequestCompleteWithInformation(Request, status, bytes_returned);
    }

    return;
}

#define STRB_BASE 0x00000008
#define DTA_BASE 0x00000001
#define CLK_BASE 0x00000002
void synccom_port_set_clock_bits(_In_ struct synccom_port *port, unsigned char *clock_data)
{
	UINT32 orig_fcr_value = 0;
	UINT32 new_fcr_value = 0;
	int j = 0; // Must be signed because we are going backwards through the array
	int i = 0; // Must be signed because we are going backwards through the array
	unsigned strb_value = STRB_BASE;
	unsigned dta_value = DTA_BASE;
	unsigned clk_value = CLK_BASE;
	UINT32 *data = 0;
	unsigned data_index = 0;

#ifdef DISABLE_XTAL
	clock_data[15] &= 0xfb;
#else
	clock_data[15] |= 0x04;
#endif

	data = (UINT32 *)ExAllocatePoolWithTag(PagedPool, sizeof(UINT32)* 323, 'stiB');
	if (data == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: ExAllocatePoolWithTag failed.", __FUNCTION__);
		return;
	}

	WdfSpinLockAcquire(port->board_settings_spinlock);
	orig_fcr_value = (UINT32)port->register_storage.FCR;
	data[data_index++] = new_fcr_value = orig_fcr_value & 0xfffff0f0;
	for (i = 19; i >= 0; i--) {
		for (j = 7; j >= 0; j--) {
			int bit = ((clock_data[i] >> j) & 1);

			if (bit)
				new_fcr_value |= dta_value; /* Set data bit */
			else
				new_fcr_value &= ~dta_value; /* Clear clock bit */

			data[data_index++] = new_fcr_value |= clk_value; /* Set clock bit */
			data[data_index++] = new_fcr_value &= ~clk_value; /* Clear clock bit */
		}
	}

	new_fcr_value = orig_fcr_value & 0xfffff0f0;

	new_fcr_value |= strb_value; /* Set strobe bit */
	new_fcr_value &= ~clk_value; /* Clear clock bit */

	data[data_index++] = new_fcr_value;
	data[data_index++] = orig_fcr_value;
	synccom_port_set_register_rep(port, FPGA_UPPER_ADDRESS, FCR_OFFSET, data, data_index);

	WdfSpinLockRelease(port->board_settings_spinlock);

	ExFreePoolWithTag(data, 'stiB');
}

NTSTATUS synccom_port_program_firmware(_In_ struct synccom_port *port, unsigned char *firmware_line, size_t data_size)
{
	WDFUSBPIPE write_pipe;
	WDFREQUEST firmware_request;
	WDFMEMORY firmware_command;
	WDF_OBJECT_ATTRIBUTES attributes;
	unsigned char *firmware_buffer = 0;
	NTSTATUS status = STATUS_SUCCESS;

	UNUSED(firmware_line);
	write_pipe = port->register_write_pipe;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->register_write_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(write_pipe), &firmware_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		WdfObjectDelete(firmware_request);
		return status;
	}
	attributes.ParentObject = firmware_request;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, data_size+1, &firmware_command, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new request.\n", __FUNCTION__);
		WdfObjectDelete(firmware_request);
		return status;
	}
	status = WdfMemoryCopyFromBuffer(firmware_command, 1, (void *)firmware_line, data_size);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot copy buffer to memory.\n", __FUNCTION__);
		WdfObjectDelete(firmware_request);
		return status;
	}
	firmware_buffer = WdfMemoryGetBuffer(firmware_command, NULL);
	firmware_buffer[0] = 0x06;
	status = WdfUsbTargetPipeFormatRequestForWrite(write_pipe, firmware_request, firmware_command, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for write.\n", __FUNCTION__);
		WdfObjectDelete(firmware_request);
		return status;
	}
	WdfRequestSetCompletionRoutine(firmware_request, basic_completion, write_pipe);
	WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(firmware_request, WdfUsbTargetPipeGetIoTarget(write_pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		status = WdfRequestGetStatus(firmware_request);
		WdfObjectDelete(firmware_request);
	}
	WdfSpinLockRelease(port->request_spinlock);

	return status;
}

NTSTATUS synccom_port_set_registers(_In_ struct synccom_port *port, const struct synccom_registers *regs)
{
	unsigned i = 0;

	for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
		unsigned char register_offset = (unsigned char)i * 4;
		if (is_read_only_register(register_offset) || ((synccom_register *)regs)[i] < 0) continue;
		if (register_offset > MAX_OFFSET) synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS, FCR_OFFSET, (UINT32)(((synccom_register *)regs)[i]), basic_completion);
		else synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, register_offset, (UINT32)(((synccom_register *)regs)[i]), basic_completion);
		if (!is_write_only_register(register_offset) && register_offset > MAX_OFFSET)  synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS, FCR_OFFSET, register_completion, port);
		else if (!is_write_only_register(register_offset)) synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, register_offset, register_completion, port);
	}

	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_set_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, UINT32 value, EVT_WDF_REQUEST_COMPLETION_ROUTINE write_return)
{
	WDFREQUEST write_request;
	WDFMEMORY write_memory;
	WDF_OBJECT_ATTRIBUTES  attributes;
	NTSTATUS status;
	WDFUSBPIPE write_pipe;
	unsigned char *write_buffer = 0;

	UNUSED(write_return);

	write_pipe = port->register_write_pipe;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = write_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(write_pipe), &write_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new write request.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	attributes.ParentObject = write_request;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, 7, &write_memory, &write_buffer);
	if (!NT_SUCCESS(status)){
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: WdfMemoryCreate failed! status: 0x%x\n", __FUNCTION__, status);
		return status;
	}
	write_buffer[0] = SYNCCOM_WRITE_REGISTER;
	write_buffer[1] = offset;
	write_buffer[2] = address << 1;

#ifdef __BIG_ENDIAN
#else
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "%s: Swapping endianness! Little endian!", __FUNCTION__);
	value = _BYTESWAP_UINT32(value);
#endif
	memcpy(&write_buffer[3], &value, sizeof(UINT32));
	status = WdfUsbTargetPipeFormatRequestForWrite(write_pipe, write_request, write_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for write.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		return status;
	}
	WdfRequestSetCompletionRoutine(write_request, write_return, write_pipe);
	WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(write_request, WdfUsbTargetPipeGetIoTarget(write_pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		WdfSpinLockRelease(port->request_spinlock);
		status = WdfRequestGetStatus(write_request);
		WdfObjectDelete(write_request);
		return status;
	}
	WdfSpinLockRelease(port->request_spinlock);

	return status;
}

void synccom_port_set_register_rep(_In_ struct synccom_port *port, unsigned char offset, unsigned char address, const UINT32 *buf, unsigned write_count)
{
	unsigned i = 0;
	const UINT32 *outgoing_data;
	NTSTATUS status = STATUS_SUCCESS;

	outgoing_data = buf;
	for (i = 0; i < write_count; i++)
	{
		status = synccom_port_set_register_async(port, offset, address, outgoing_data[i], basic_completion);
		if (!NT_SUCCESS(status))
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: failed iteration: %d, status: %!STATUS!\n", __FUNCTION__, i, status);
	}

}
 
void synccom_port_get_registers(struct synccom_port *port, struct synccom_registers *regs)
{
	unsigned i = 0;

	for (i = 0; i < sizeof(*regs) / sizeof(synccom_register); i++) {
		unsigned char register_offset = (unsigned char)i * 4;
		if (((synccom_register *)regs)[i] != SYNCCOM_UPDATE_VALUE) continue;
		if (is_write_only_register((unsigned char)i * 4)) continue;
		((synccom_register *)regs)[i] = ((synccom_register *)&port->register_storage)[i];
		synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, register_offset, register_completion, port);
	}
}

synccom_register synccom_port_get_register_async(struct synccom_port *port, unsigned char offset, unsigned char address, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context)
{
	WDFMEMORY write_memory, read_memory;
	NTSTATUS status = STATUS_SUCCESS;
	WDFUSBPIPE write_pipe, read_pipe;
	unsigned char *write_buffer = NULL;
	WDF_OBJECT_ATTRIBUTES  attributes;
	WDFREQUEST write_request, read_request;

	write_pipe = port->register_write_pipe;
	read_pipe = port->register_read_pipe;
	
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = read_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(read_pipe), &read_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new read request.\n", __FUNCTION__);
		WdfObjectDelete(read_request);
		return status;
	}
	
	attributes.ParentObject = write_pipe;
	status = WdfRequestCreate(&attributes, WdfUsbTargetPipeGetIoTarget(write_pipe), &write_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot create new write request.\n", __FUNCTION__);
		WdfObjectDelete(read_request);
		WdfObjectDelete(write_request);
		return status;
	}
	attributes.ParentObject = write_request;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, 3, &write_memory, &write_buffer);
	if (!NT_SUCCESS(status)){
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: WdfMemoryCreate failed! status: 0x%x\n", __FUNCTION__, status);
		return status;
	}
	
	attributes.ParentObject = read_request;
	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, 6, &read_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: WdfMemoryCreate failed! status: 0x%x\n", __FUNCTION__, status);
		return status;
	}
	
	write_buffer[0] = SYNCCOM_READ_WITH_ADDRESS;
	write_buffer[1] = offset;
	write_buffer[2] = address << 1;
	
	status = WdfUsbTargetPipeFormatRequestForWrite(write_pipe, write_request, write_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for write.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		WdfObjectDelete(read_request);
		return status;
	}
	
	status = WdfUsbTargetPipeFormatRequestForRead(read_pipe, read_request, read_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for read.\n", __FUNCTION__);
		WdfObjectDelete(write_request);
		WdfObjectDelete(read_request);
		return status;
	}
	
	WdfRequestSetCompletionRoutine(write_request, basic_completion, write_pipe);
	WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(write_request, WdfUsbTargetPipeGetIoTarget(write_pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		WdfSpinLockRelease(port->request_spinlock);
		status = WdfRequestGetStatus(write_request);
		WdfObjectDelete(write_request);
		WdfObjectDelete(read_request);
		return status;
	}
	WdfSpinLockRelease(port->request_spinlock);
	
	WdfSpinLockAcquire(port->request_spinlock);
	WdfRequestSetCompletionRoutine(read_request, read_return, Context);
	if (WdfRequestSend(read_request, WdfUsbTargetPipeGetIoTarget(read_pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		WdfSpinLockRelease(port->request_spinlock);
		status = WdfRequestGetStatus(read_request);
		status = WdfRequestGetStatus(write_request);
		WdfObjectDelete(write_request);
		WdfObjectDelete(read_request);
		return status;
	}
	WdfSpinLockRelease(port->request_spinlock);

	return status;
}

synccom_register synccom_port_read_data_async(struct synccom_port *port, EVT_WDF_REQUEST_COMPLETION_ROUTINE read_return, WDFCONTEXT Context)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFUSBPIPE read_pipe;

	return_val_if_untrue(port, 0);

	read_pipe = port->data_read_pipe;
	status = WdfUsbTargetPipeFormatRequestForRead(read_pipe, port->data_read_request, port->data_read_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "%s: Cannot format request for read.\n", __FUNCTION__);
		return status;
	}

	WdfRequestSetCompletionRoutine(port->data_read_request, read_return, Context);
	WdfSpinLockAcquire(port->request_spinlock);
	if (WdfRequestSend(port->data_read_request, WdfUsbTargetPipeGetIoTarget(read_pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		WdfSpinLockRelease(port->request_spinlock);
		status = WdfRequestGetStatus(port->data_read_request);
		TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTL, "%s: WdfRequestSend failed! %!STATUS!", __FUNCTION__, status);
		return status;
	}
	WdfSpinLockRelease(port->request_spinlock);

	return status;
}

void synccom_port_set_memory_cap(struct synccom_port *port, struct synccom_memory_cap *value)
{
	return_if_untrue(port);
	return_if_untrue(value);

	if (value->input >= 0) {
		if (port->memory_cap.input != value->input) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Memory cap (input) %i => %i", port->memory_cap.input, value->input);
		}
		else {
			TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Memory cap (input) = %i", value->input);
		}

		port->memory_cap.input = value->input;
	}

	if (value->output >= 0) {
		if (port->memory_cap.output != value->output) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Memory cap (output) %i => %i", port->memory_cap.output, value->output);
		}
		else {
			TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Memory cap (output) = %i", value->output);
		}

		port->memory_cap.output = value->output;
	}
}

void synccom_port_set_append_status(struct synccom_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->append_status != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Append status %i => %i", port->append_status, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Append status = %i", value);
	}

	port->append_status = (value) ? 1 : 0;
}

BOOLEAN synccom_port_get_append_status(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return !synccom_port_is_streaming(port) && port->append_status;
}

void synccom_port_set_append_timestamp(struct synccom_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->append_timestamp != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Append timestamp %i => %i", port->append_timestamp, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Append timestamp = %i", value);
	}

	port->append_timestamp = (value) ? 1 : 0;
}

BOOLEAN synccom_port_get_append_timestamp(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return !synccom_port_is_streaming(port) && port->append_timestamp;
}

void synccom_port_set_ignore_timeout(struct synccom_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->ignore_timeout != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Ignore timeout %i => %i", port->ignore_timeout, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Ignore timeout = %i", value);
	}

	port->ignore_timeout = (value) ? TRUE : FALSE;
}

BOOLEAN synccom_port_get_ignore_timeout(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->ignore_timeout;
}

UINT32 synccom_port_get_firmware_rev(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);
	return port->firmware_rev;
}

void synccom_port_set_rx_multiple(struct synccom_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->rx_multiple != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Receive multiple %i => %i", port->rx_multiple, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Receive multiple = %i", value);
	}

	port->rx_multiple = (value) ? 1 : 0;
}

BOOLEAN synccom_port_get_rx_multiple(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->rx_multiple;
}

unsigned synccom_port_is_streaming(struct synccom_port *port)
{
	unsigned transparent_mode = 0;
	unsigned xsync_mode = 0;
	unsigned rlc_mode = 0;
	unsigned fsc_mode = 0;
	unsigned ntb = 0;

	return_val_if_untrue(port, 0);

	transparent_mode = ((port->register_storage.CCR0 & 0x3) == 0x2) ? 1 : 0;
	xsync_mode = ((port->register_storage.CCR0 & 0x3) == 0x1) ? 1 : 0;
	rlc_mode = (port->register_storage.CCR2 & 0xffff0000) ? 1 : 0;
	fsc_mode = (port->register_storage.CCR0 & 0x700) ? 1 : 0;
	ntb = (port->register_storage.CCR0 & 0x70000) >> 16;

	return ((transparent_mode || xsync_mode) && !(rlc_mode || fsc_mode || ntb)) ? 1 : 0;
}

NTSTATUS synccom_port_execute_RRES(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, CMDR_OFFSET, 0x00020000, basic_completion);
	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_execute_TRES(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	synccom_port_set_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, CMDR_OFFSET, 0x08000000, basic_completion);
	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_purge_tx(struct synccom_port *port)
{
	int error_code = 0;

	return_val_if_untrue(port, 0);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Purging transmit data.", __FUNCTION__);

	WdfSpinLockAcquire(port->board_tx_spinlock);
	error_code = synccom_port_execute_TRES(port);
	WdfSpinLockRelease(port->board_tx_spinlock);

	if (error_code < 0)
		return error_code;

	WdfSpinLockAcquire(port->queued_oframes_spinlock);
	synccom_flist_clear(&port->queued_oframes);
	WdfSpinLockRelease(port->queued_oframes_spinlock);

	WdfSpinLockAcquire(port->pending_oframe_spinlock);
	if (port->pending_oframe) {
		synccom_frame_delete(port->pending_oframe);
		port->pending_oframe = 0;
	}
	WdfSpinLockRelease(port->pending_oframe_spinlock);

	WdfIoQueuePurgeSynchronously(port->write_queue);
	WdfIoQueuePurgeSynchronously(port->write_queue2);
	WdfIoQueueStart(port->write_queue);
	WdfIoQueueStart(port->write_queue2);

	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_purge_rx(struct synccom_port *port)

{
	NTSTATUS status;

	return_val_if_untrue(port, 0);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Purging receive data.", __FUNCTION__);

	WdfSpinLockAcquire(port->board_rx_spinlock);
	status = synccom_port_execute_RRES(port);
	WdfSpinLockRelease(port->board_rx_spinlock);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: failed %!STATUS!", __FUNCTION__, status);
		return status;
	}

	WdfSpinLockAcquire(port->queued_iframes_spinlock);
	synccom_flist_clear(&port->queued_iframes);
	WdfSpinLockRelease(port->queued_iframes_spinlock);

	WdfSpinLockAcquire(port->istream_spinlock);
	synccom_frame_clear(port->istream);
	WdfSpinLockRelease(port->istream_spinlock);

	WdfSpinLockAcquire(port->pending_iframes_spinlock);
	synccom_flist_clear(&port->pending_iframes);
	WdfSpinLockRelease(port->pending_iframes_spinlock);
	
	WdfIoQueuePurgeSynchronously(port->read_queue);
	WdfIoQueuePurgeSynchronously(port->read_queue2);
	WdfIoQueueStart(port->read_queue);
	WdfIoQueueStart(port->read_queue2);

	return STATUS_SUCCESS;
}

unsigned synccom_port_has_incoming_data(struct synccom_port *port)
{
	unsigned status = 0;

	return_val_if_untrue(port, 0);

	if (synccom_port_is_streaming(port)) {
		status = (synccom_frame_is_empty(port->istream)) ? 0 : 1;
	}
	else {
		WdfSpinLockAcquire(port->queued_iframes_spinlock);
		if (synccom_flist_is_empty(&port->queued_iframes) == FALSE)
			status = 1;
		WdfSpinLockRelease(port->queued_iframes_spinlock);
	}
	return status;
}

unsigned synccom_port_get_input_memory_cap(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->memory_cap.input;
}

unsigned synccom_port_get_output_memory_cap(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->memory_cap.output;
}

unsigned synccom_port_get_output_memory_usage(struct synccom_port *port)
{
	unsigned value = 0;

	return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->queued_oframes_spinlock);
	value = port->queued_oframes.estimated_memory_usage;
	WdfSpinLockRelease(port->queued_oframes_spinlock);

	WdfSpinLockAcquire(port->pending_oframe_spinlock);
	if (port->pending_oframe)
		value += synccom_frame_get_length(port->pending_oframe);
	WdfSpinLockRelease(port->pending_oframe_spinlock);

	return value;
}

unsigned synccom_port_get_input_memory_usage(struct synccom_port *port)
{
	unsigned value = 0;

	return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->queued_iframes_spinlock);
	value = port->queued_iframes.estimated_memory_usage;
	WdfSpinLockRelease(port->queued_iframes_spinlock);

	WdfSpinLockAcquire(port->istream_spinlock);
	value += synccom_frame_get_length(port->istream);
	WdfSpinLockRelease(port->istream_spinlock);
	
	WdfSpinLockAcquire(port->pending_iframes_spinlock);
	value += port->pending_iframes.estimated_memory_usage;
	WdfSpinLockRelease(port->pending_iframes_spinlock);

	return value;
}

unsigned synccom_port_get_tx_modifiers(struct synccom_port *port)
{
	return_val_if_untrue(port, 0);

	return port->tx_modifiers;
}

NTSTATUS synccom_port_set_tx_modifiers(struct synccom_port *port, int value)
{
	return_val_if_untrue(port, 0);

	switch (value) {
	case XF:
	case XF | TXT:
	case XF | TXEXT:
	case XREP:
	case XREP | TXT:
		if (port->tx_modifiers != value) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Transmit modifiers 0x%x => 0x%x", port->tx_modifiers, value);
		}
		else {
			TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "Transmit modifiers = 0x%x", value);
		}

		port->tx_modifiers = value;

		break;

	default:
		TraceEvents(TRACE_LEVEL_WARNING, DBG_PNP, "Transmit modifiers (invalid value 0x%x)", value);

		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_get_port_num(struct synccom_port *port, unsigned *port_num)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG port_num_long;

	RtlInitUnicodeString(&key_str, L"PortNumber");

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE,
		STANDARD_RIGHTS_ALL,
		WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = WdfRegistryQueryULong(devkey, &key_str, &port_num_long);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfRegistryQueryULong failed %!STATUS!", status);
		return status;
	}

	*port_num = (unsigned)port_num_long;

	WdfRegistryClose(devkey);

	return status;
}

NTSTATUS synccom_port_set_port_num(struct synccom_port *port, unsigned value)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"PortNumber");

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = WdfRegistryAssignULong(devkey, &key_str, value);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfRegistryAssignULong failed %!STATUS!", status);
		return status;
	}

	WdfRegistryClose(devkey);

	return status;
}