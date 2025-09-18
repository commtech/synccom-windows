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
#include "utils.h"
#include "isr.h"
#include "port.h"
#include "flist.h"
#include "frame.h"
#include "device.h"

#include <devpkey.h>

#if defined(EVENT_TRACING)
#include "device.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SyncComEvtDeviceAdd)
#pragma alloc_text(PAGE, SyncComEvtDevicePrepareHardware)
#pragma alloc_text(PAGE, SyncComEvtDeviceD0Exit)
#pragma alloc_text(PAGE, SelectInterfaces)
#pragma alloc_text(PAGE, OsrFxSetPowerPolicy)
#endif

NTSTATUS SetupUsb(_In_ struct synccom_port* port);
NTSTATUS synccom_port_set_friendly_name(_In_ WDFDEVICE Device, unsigned portnum);
NTSTATUS synccom_port_setget_default_settings(struct synccom_port* port);
NTSTATUS synccom_port_setget_default_registers(struct synccom_port* port);

NTSTATUS SyncComEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	struct synccom_port* port = 0;

	PAGED_CODE();

	port = synccom_port_new(Driver, DeviceInit);
	if (!port)
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "%s: Error: no valid port created!\n", __FUNCTION__);
		return STATUS_INTERNAL_ERROR;
	}

	return STATUS_SUCCESS;
}

struct synccom_port* synccom_port_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS							status = STATUS_SUCCESS;
	WDF_PNPPOWER_EVENT_CALLBACKS        pnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES               attributes;
	WDFDEVICE                           device;
	WDF_DEVICE_PNP_CAPABILITIES         pnpCaps;
	WDF_IO_QUEUE_CONFIG					queue_config;
	WDF_DEVICE_STATE					device_state;
	WCHAR								dos_name_buffer[30];
	UNICODE_STRING						dos_name;

	struct synccom_port* port = 0;
	static int instance = 0;
	unsigned default_port_num = 0, port_num = 0;
	WCHAR device_name_buffer[50];
	UNICODE_STRING device_name;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	WdfSpinLockAcquire(port_num_spinlock);
	default_port_num = instance;
	instance++;
	WdfSpinLockRelease(port_num_spinlock);

	RtlInitEmptyUnicodeString(&device_name, device_name_buffer, sizeof(device_name_buffer));
	status = RtlUnicodeStringPrintf(&device_name, L"\\Device\\SYNCCOM%i", default_port_num);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: RtlUnicodeStringPrintf failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = WdfDeviceInitAssignName(DeviceInit, &device_name);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceInitAssignName failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Device has been named with WdfDeviceInitAssignName: %wZ!", __FUNCTION__, &device_name);
	// https://msdn.microsoft.com/en-us/library/windows/hardware/ff563667(v=vs.85).aspx
	// Note: When using SDDL for device objects, your driver must link against Wdmsec.lib.
	status = WdfDeviceInitAssignSDDLString(DeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceInitAssignSDDLString failed %!STATUS!", status);
		return 0;
	}

	// I don't know if the following is necessary - it assigns a 'DeviceType'.
	// But this is a USB driver. Does it need a DeviceType? DeviceClass?
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_SERIAL_PORT);
	WdfDeviceInitSetDeviceClass(DeviceInit, (LPGUID)&GUID_DEVCLASS_SYNCCOM);

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = SyncComEvtDevicePrepareHardware;
	pnpPowerCallbacks.EvtDeviceReleaseHardware = SyncComEvtDeviceReleaseHardware;
	pnpPowerCallbacks.EvtDeviceD0Entry = SyncComEvtDeviceD0Entry;
	pnpPowerCallbacks.EvtDeviceD0Exit = SyncComEvtDeviceD0Exit;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, SYNCCOM_PORT);
	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceCreate failed with Status code %X\n", __FUNCTION__, status);
		return 0;
	}

	port = GetPortContext(device);
	port->device = device;
	port->istream = synccom_frame_new(port);

	WDF_DEVICE_STATE_INIT(&device_state);
	device_state.DontDisplayInUI = WdfFalse;
	WdfDeviceSetDeviceState(port->device, &device_state);

	status = SetupUsb(port);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "SetupUsb failed %X\n", status);
		return 0;
	}
	port->fx2_firmware_rev = 0;
	synccom_port_get_fx2_firmware(port, register_completion, port);

	port_num = default_port_num;
	status = synccom_port_set_port_num(port, port_num);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "synccom_port_set_port_num failed %X\n", status);
		return 0;
	}
	port->port_number = port_num;


	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.SurpriseRemovalOK = WdfTrue;
	pnpCaps.UniqueID = WdfTrue;
	pnpCaps.UINumber = port_num;
	WdfDeviceSetPnpCapabilities(port->device, &pnpCaps);

	///////
		/*
		RtlInitEmptyUnicodeString(&dos_name, dos_name_buffer, sizeof(dos_name_buffer));
		status = RtlUnicodeStringPrintf(&dos_name, L"\\DosDevices\\SYNCCOM%i", port_num);
		if (!NT_SUCCESS(status)) {
			WdfObjectDelete(port->device);
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: RtlUnicodeStringPrintf failed %!STATUS!", __FUNCTION__, status);
			return 0;
		}

		status = WdfDeviceCreateSymbolicLink(port->device, &dos_name);
		if (!NT_SUCCESS(status)) {
			WdfObjectDelete(port->device);
			TraceEvents(TRACE_LEVEL_WARNING, DBG_PNP, "%s: WdfDeviceCreateSymbolicLink failed %!STATUS!", __FUNCTION__, status);
			return 0;
		}
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Device has a symbolic link with WdfDeviceCreateSymbolicLink: %wZ!", __FUNCTION__, &dos_name);
		*/

	status = synccom_port_set_friendly_name(port->device, port->port_number);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "synccom_port_set_friendly_name failed 0x%X\n", status);
	}

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoDeviceControl = SyncComEvtIoDeviceControl;

	__analysis_assume(queue_config.EvtIoStop != 0);
	status = WdfIoQueueCreate(port->device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &port->ioctl_queue);
	__analysis_assume(queue_config.EvtIoStop == 0);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfIoQueueCreate failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}
	status = WdfDeviceConfigureRequestDispatching(port->device, port->ioctl_queue, WdfRequestTypeDeviceControl);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceConfigureRequestDispatching failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoWrite = SyncComEvtIoWrite;
	queue_config.EvtIoStop = SyncComEvtIoStop;

	status = WdfIoQueueCreate(port->device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &port->write_queue);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfIoQueueCreate failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = WdfDeviceConfigureRequestDispatching(port->device, port->write_queue, WdfRequestTypeWrite);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceConfigureRequestDispatching failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(port->device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &port->write_queue2);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfIoQueueCreate failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoRead = SyncComEvtIoRead;
	queue_config.EvtIoStop = SyncComEvtIoStop;

	status = WdfIoQueueCreate(port->device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &port->read_queue);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfIoQueueCreate failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}
	status = WdfDeviceConfigureRequestDispatching(port->device, port->read_queue, WdfRequestTypeRead);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceConfigureRequestDispatching failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(port->device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &port->read_queue2);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfIoQueueCreate failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_DEVINTERFACE_SYNCCOM, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceCreateDeviceInterface failed  %!STATUS!\n", __FUNCTION__, status);
		return 0;
	}

	RtlInitEmptyUnicodeString(&dos_name, dos_name_buffer, sizeof(dos_name_buffer));
	status = RtlUnicodeStringPrintf(&dos_name, L"\\DosDevices\\SYNCCOM%i", port_num);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: RtlUnicodeStringPrintf failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = WdfDeviceCreateSymbolicLink(port->device, &dos_name);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_WARNING, DBG_PNP, "%s: WdfDeviceCreateSymbolicLink failed %!STATUS!", __FUNCTION__, status);
		return 0;
	}
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: Device has a symbolic link with WdfDeviceCreateSymbolicLink: %wZ!", __FUNCTION__, &dos_name);

	status = setup_spinlocks(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Failed to set up spinlocks!  %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = setup_request(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Failed to set up requests!  %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = setup_memory(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Failed to set up memory!  %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = setup_dpc(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Failed to set up DPCs!  %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	status = setup_timer(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Failed to set up timer!  %!STATUS!", __FUNCTION__, status);
		return 0;
	}

	synccom_flist_init(&port->queued_oframes);
	synccom_flist_init(&port->queued_iframes);
	synccom_flist_init(&port->pending_iframes);

	return port;
}

NTSTATUS setup_spinlocks(_In_ struct synccom_port* port)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES attributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->device;

	status = WdfSpinLockCreate(&attributes, &port->request_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->board_settings_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->board_rx_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->board_tx_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->istream_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->queued_iframes_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->pending_iframes_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->frame_size_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->pending_oframe_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	status = WdfSpinLockCreate(&attributes, &port->queued_oframes_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfSpinLockCreate failed %!STATUS!", status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS setup_request(_In_ struct synccom_port* port)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, WdfUsbTargetPipeGetIoTarget(port->data_read_pipe), &port->data_read_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Unable to make ReadRequest! %!STATUS!", __FUNCTION__, status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS setup_memory(_In_ struct synccom_port* port)
{
	NTSTATUS							status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES               attributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->device;

	status = WdfMemoryCreate(&attributes, NonPagedPool, 0, 512, &port->data_read_memory, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfMemoryCreate failed! status: 0x%x\n", __FUNCTION__, status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS setup_dpc(_In_ struct synccom_port* port)
{
	NTSTATUS							status = STATUS_SUCCESS;
	WDF_DPC_CONFIG						dpcConfig;
	WDF_OBJECT_ATTRIBUTES				dpcAttributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	WDF_DPC_CONFIG_INIT(&dpcConfig, &oframe_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->oframe_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: oframe_dpc failed %!STATUS!", __FUNCTION__, status);
		return status;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &iframe_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->iframe_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: iframe_dpc failed %!STATUS!", __FUNCTION__, status);
		return status;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &SynccomProcessRead);
	dpcConfig.AutomaticSerialization = TRUE;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->process_read_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: process_read_dpc failed %!STATUS!", __FUNCTION__, status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS setup_timer(_In_ struct synccom_port* port) {
	NTSTATUS status = STATUS_SUCCESS;
	WDF_TIMER_CONFIG  timerConfig;
	WDF_OBJECT_ATTRIBUTES  timerAttributes;

	WDF_TIMER_CONFIG_INIT(&timerConfig, timer_handler);
	timerConfig.Period = TIMER_DELAY_MS;
	timerConfig.TolerableDelay = TIMER_DELAY_MS;

	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
	timerAttributes.ParentObject = port->device;

	status = WdfTimerCreate(&timerConfig, &timerAttributes, &port->timer);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfTimerCreate failed %!STATUS!", status);
		return status;
	}

	return status;
}

NTSTATUS SyncComEvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourceList, WDFCMRESLIST ResourceListTranslated)
{
	NTSTATUS                            status = STATUS_SUCCESS;
	//WDF_USB_DEVICE_INFORMATION          deviceInfo;
	//ULONG                               wait_wake_enable;
	struct synccom_memory_cap			memory_cap;
	unsigned char						clock_bits[20] = DEFAULT_CLOCK_BITS;
	struct synccom_port* port = 0;
	WDF_USB_CONTINUOUS_READER_CONFIG	readerConfig;

	UNREFERENCED_PARAMETER(ResourceList);
	UNREFERENCED_PARAMETER(ResourceListTranslated);

	PAGED_CODE();
	port = GetPortContext(Device);

	WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&readerConfig, port_received_data, port, 512);
	readerConfig.NumPendingReads = 1;
	readerConfig.EvtUsbTargetPipeReadersFailed = FX3EvtReadFailed;
	status = WdfUsbTargetPipeConfigContinuousReader(port->data_read_pipe, &readerConfig);

	memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
	memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;
	synccom_port_set_memory_cap(port, &memory_cap);

	port->pending_oframe = 0;
	port->writes_in_flight = 0;
	port->pending_frame_size_reads = 0;

	synccom_port_setget_default_settings(port);
	synccom_port_setget_default_registers(port);

	synccom_port_set_clock_bits(port, clock_bits);

	synccom_port_purge_rx(port);
	synccom_port_purge_tx(port);
	synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS + SYNCCOM_UPPER_OFFSET, VSTR_OFFSET, register_completion, port);
	synccom_port_get_register_async(port, FPGA_UPPER_ADDRESS, 0x00, register_completion, port);
	if (synccom_port_can_support_nonvolatile(port)) synccom_port_get_nonvolatile(port, register_completion, port);

	WdfTimerStart(port->timer, WDF_ABS_TIMEOUT_IN_MS(TIMER_DELAY_MS));

	return status;
}

NTSTATUS SyncComEvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct synccom_port* port = 0;
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	port = GetPortContext(Device);
	return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->istream_spinlock);
	synccom_frame_delete(port->istream);
	WdfSpinLockRelease(port->istream_spinlock);

	WdfSpinLockAcquire(port->pending_iframes_spinlock);
	synccom_flist_delete(&port->pending_iframes);
	WdfSpinLockRelease(port->pending_iframes_spinlock);

	WdfSpinLockAcquire(port->pending_oframe_spinlock);
	synccom_frame_delete(port->pending_oframe);
	WdfSpinLockRelease(port->pending_oframe_spinlock);

	WdfSpinLockAcquire(port->queued_iframes_spinlock);
	synccom_flist_delete(&port->queued_iframes);
	WdfSpinLockRelease(port->queued_iframes_spinlock);

	WdfSpinLockAcquire(port->queued_oframes_spinlock);
	synccom_flist_delete(&port->queued_oframes);
	WdfSpinLockRelease(port->queued_oframes_spinlock);

	return status;
}

NTSTATUS SyncComEvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
	NTSTATUS                status = STATUS_SUCCESS;
	struct synccom_port* port = 0;

	UNREFERENCED_PARAMETER(PreviousState);

	PAGED_CODE();
	port = GetPortContext(Device);
	return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
	status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(port->data_read_pipe));
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_POWER, "%!FUNC! Could not start data_read_pipe: failed 0x%x", status);
	}

	return status;
}

NTSTATUS SyncComEvtDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState)
{
	struct synccom_port* port = 0;
	UNREFERENCED_PARAMETER(TargetState);
	PAGED_CODE();
	port = GetPortContext(Device);

	return_val_if_untrue(port, STATUS_SUCCESS);
	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(port->data_read_pipe), WdfIoTargetCancelSentIo);

	return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS OsrFxSetPowerPolicy(_In_ struct synccom_port* port)
{
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
	NTSTATUS    status = STATUS_SUCCESS;

	PAGED_CODE();

	//
	// Init the idle policy structure.
	//
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
	idleSettings.IdleTimeout = 10000; // 10-sec

	status = WdfDeviceAssignS0IdleSettings(port->device, &idleSettings);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceSetPowerPolicyS0IdlePolicy failed %x\n", __FUNCTION__, status);
		return status;
	}

	//
	// Init wait-wake policy structure.
	//
	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

	status = WdfDeviceAssignSxWakeSettings(port->device, &wakeSettings);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceAssignSxWakeSettings failed %x\n", __FUNCTION__, status);
		return status;
	}

	return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS SelectInterfaces(_In_ struct synccom_port* port)
{
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
	NTSTATUS                            status = STATUS_SUCCESS;
	WDFUSBPIPE                          pipe;
	WDF_USB_PIPE_INFORMATION            pipeInfo;
	UCHAR                               index;
	UCHAR                               numberConfiguredPipes;
	PAGED_CODE();


	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

	status = WdfUsbTargetDeviceSelectConfig(port->usb_device, WDF_NO_OBJECT_ATTRIBUTES, &configParams);
	if (!NT_SUCCESS(status)) {

		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfUsbTargetDeviceSelectConfig failed %!STATUS! \n", __FUNCTION__, status);
		return status;
	}

	port->usb_interface = configParams.Types.SingleInterface.ConfiguredUsbInterface;
	numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

	for (index = 0; index < numberConfiguredPipes; index++) {

		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		pipe = WdfUsbInterfaceGetConfiguredPipe(port->usb_interface, index, &pipeInfo);
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

		if (DATA_READ_ENDPOINT == pipeInfo.EndpointAddress && WdfUsbTargetPipeIsInEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: data_read_pipe is 0x%p\n", __FUNCTION__, pipe);
			port->data_read_pipe = pipe;
		}

		if (DATA_WRITE_ENDPOINT == pipeInfo.EndpointAddress && WdfUsbTargetPipeIsOutEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: data_write_pipe is 0x%p\n", __FUNCTION__, pipe);
			port->data_write_pipe = pipe;
		}

		if (REGISTER_READ_ENDPOINT == pipeInfo.EndpointAddress && WdfUsbTargetPipeIsInEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: register_read_pipe is 0x%p\n", __FUNCTION__, pipe);
			port->register_read_pipe = pipe;
		}

		if (REGISTER_WRITE_ENDPOINT == pipeInfo.EndpointAddress && WdfUsbTargetPipeIsOutEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "%s: register_write_pipe is 0x%p\n", __FUNCTION__, pipe);
			port->register_write_pipe = pipe;
		}

	}

	if (!(port->data_read_pipe
		&& port->data_write_pipe
		&& port->register_read_pipe
		&& port->register_write_pipe)) {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Device is not CONFIGURED properly %!STATUS!\n", __FUNCTION__, status);
		return status;
	}

	return status;
}

NTSTATUS SetupUsb(_In_ struct synccom_port* port)
{
	NTSTATUS                            status = STATUS_SUCCESS;
	WDF_USB_DEVICE_INFORMATION          deviceInfo;
	ULONG                               wait_wake_enable;

	wait_wake_enable = FALSE;

	if (port->usb_device == NULL) {
		WDF_USB_DEVICE_CREATE_CONFIG config;

		WDF_USB_DEVICE_CREATE_CONFIG_INIT(&config, USBD_CLIENT_CONTRACT_VERSION_602);

		status = WdfUsbTargetDeviceCreateWithParameters(port->device, &config, WDF_NO_OBJECT_ATTRIBUTES, &port->usb_device);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfUsbTargetDeviceCreateWithParameters failed with Status code 0x%x\n", __FUNCTION__, status);
			return status;
		}
	}

	WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);
	status = WdfUsbTargetDeviceRetrieveInformation(port->usb_device, &deviceInfo);
	if (NT_SUCCESS(status)) {
		port->usb_traits = deviceInfo.Traits;
		wait_wake_enable = deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: IsDeviceHighSpeed: %s\n", __FUNCTION__, (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? "TRUE" : "FALSE");
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: IsDeviceSelfPowered: %s\n", __FUNCTION__, (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED) ? "TRUE" : "FALSE");
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: IsDeviceRemoteWakeable: %s\n", __FUNCTION__, wait_wake_enable ? "TRUE" : "FALSE");
	}
	else {
		port->usb_traits = 0;
	}

	status = SelectInterfaces(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: SelectInterfaces failed 0x%x\n", __FUNCTION__, status);
		return status;
	}

	if (wait_wake_enable) {
		status = OsrFxSetPowerPolicy(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: OsrFxSetPowerPolicy failed  0x%x\n", __FUNCTION__, status);
			return status;
		}
	}
	return STATUS_SUCCESS;
}

#define FRIENDLYNAME_SIZE 256
NTSTATUS synccom_port_set_friendly_name(_In_ WDFDEVICE Device, unsigned portnum)
{
	NTSTATUS status;
	WDF_DEVICE_PROPERTY_DATA dpd;
	WCHAR friendlyName[FRIENDLYNAME_SIZE] = { 0 };
	int characters_written = 0;

	WDF_DEVICE_PROPERTY_DATA_INIT(&dpd, &DEVPKEY_Device_FriendlyName);
	dpd.Lcid = LOCALE_NEUTRAL;
	dpd.Flags = PLUGPLAY_PROPERTY_PERSISTENT;
	characters_written = swprintf_s(friendlyName, FRIENDLYNAME_SIZE, L"SYNCCOM Port (SYNCCOM%i)", portnum);
	if (characters_written < 0) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "swprintf_s failed with %d\n", characters_written);
		return STATUS_INVALID_PARAMETER;
	}

	status = WdfDeviceAssignProperty(Device, &dpd, DEVPROP_TYPE_STRING, (characters_written + 1) * sizeof(WCHAR), (PVOID)&friendlyName);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfDeviceAssignProperty failed 0x%X\n", status);
		return status;
	}

	return status;
}

NTSTATUS synccom_port_setget_default_registers(struct synccom_port* port)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG value;

	SYNCCOM_REGISTERS_INIT(port->register_storage);

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfDeviceOpenRegistryKey failed %!STATUS!, setting default registers", status);
		synccom_port_set_registers(port, &port->register_storage);
		return status;
	}

	RtlInitUnicodeString(&key_str, L"FIFOT");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_FIFOT_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.FIFOT = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR0");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR0_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.CCR0 = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR1");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR1_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.CCR1 = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR2");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR2_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.CCR2 = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"BGR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BGR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.BGR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"SSR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_SSR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.SSR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"SMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_SMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.SMR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"TSR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TSR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.TSR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"TMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.TMR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"RAR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_RAR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.RAR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"RAMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_RAMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.RAMR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"PPR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_PPR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.PPR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"TCR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TCR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.TCR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"IMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_IMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.IMR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"DPLLR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_DPLLR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.DPLLR = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"FCR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_FCR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->register_storage.FCR = (unsigned)value;

	WdfRegistryClose(devkey);

	synccom_port_set_registers(port, &port->register_storage);

	return STATUS_SUCCESS;
}

NTSTATUS synccom_port_setget_default_settings(struct synccom_port* port)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG value;

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfDeviceOpenRegistryKey failed %!STATUS!, setting default max writes", status);
		port->max_pending_writes = DEFAULT_MAX_WRITES;
		return status;
	}

	RtlInitUnicodeString(&key_str, L"MAX_WRITES");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_MAX_WRITES;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	port->max_pending_writes = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"TX_MODIFIERS");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TX_MODIFIERS_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	synccom_port_set_tx_modifiers(port, (unsigned)value);

	RtlInitUnicodeString(&key_str, L"APPEND_STATUS");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_APPEND_STATUS_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	synccom_port_set_append_status(port, (BOOLEAN)value);

	RtlInitUnicodeString(&key_str, L"APPEND_TIMESTAMP");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_APPEND_TIMESTAMP_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	synccom_port_set_append_timestamp(port, (BOOLEAN)value);

	RtlInitUnicodeString(&key_str, L"IGNORE_TIMEOUT");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_IGNORE_TIMEOUT_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	synccom_port_set_ignore_timeout(port, (BOOLEAN)value);

	RtlInitUnicodeString(&key_str, L"RX_MULTIPLE");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_RX_MULTIPLE_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	synccom_port_set_rx_multiple(port, (BOOLEAN)value);

	WdfRegistryClose(devkey);
	
	return STATUS_SUCCESS;
}