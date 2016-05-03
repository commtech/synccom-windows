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

#include <driver.h>
#include <devpkey.h>
#include "utils.h"
#include "isr.h"
#include <port.h>
#include <flist.h>
#include <frame.h>
#include <device.h>

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

NTSTATUS SyncComEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
	struct synccom_port *port = 0;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "%s: Entering.\n", __FUNCTION__);
	port = synccom_port_new(Driver, DeviceInit);
	if (!port)
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "%s: Error: no valid port created!\n", __FUNCTION__);
		return STATUS_INTERNAL_ERROR;
	}
	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "%s: Exiting.\n", __FUNCTION__);
	return STATUS_SUCCESS;
}

struct synccom_port *synccom_port_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit)
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

	struct synccom_port *port = 0;
	static int instance = 0;
	unsigned port_num = 0;
	WCHAR device_name_buffer[50];
	UNICODE_STRING device_name;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "%s: Entering.\n", __FUNCTION__);

	port_num = instance;
	instance++;

	RtlInitEmptyUnicodeString(&device_name, device_name_buffer, sizeof(device_name_buffer));
	status = RtlUnicodeStringPrintf(&device_name, L"\\Device\\SYNCCOM%i", port_num);
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
	pnpPowerCallbacks.EvtDeviceD0Exit =  SyncComEvtDeviceD0Exit;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, synccom_PORT);
	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfDeviceCreate failed with Status code %!STATUS!\n", __FUNCTION__, status);
		return 0;
	}

	port = GetPortContext(device);
	port->device = device;
	port->istream = synccom_frame_new(port);

	WDF_DEVICE_STATE_INIT(&device_state);
	device_state.DontDisplayInUI = WdfFalse;
	WdfDeviceSetDeviceState(port->device, &device_state);

	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.SurpriseRemovalOK = WdfTrue;
	pnpCaps.UniqueID = WdfTrue;
	pnpCaps.UINumber = port_num;
	WdfDeviceSetPnpCapabilities(port->device, &pnpCaps);

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
	queue_config.EvtIoStop  = SyncComEvtIoStop;

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
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "%s: device name %wZ!", __FUNCTION__, &dos_name);
	// 
	// Create the lock that we use to serialize calls to ResetDevice(). As an 
	// alternative to using a WDFWAITLOCK to serialize the calls, a sequential 
	// WDFQUEUE can be created and reset IOCTLs would be forwarded to it.
	//
	/*
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;

	status = WdfWaitLockCreate(&attributes, &pDevContext->ResetDeviceWaitLock);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfWaitLockCreate failed  %!STATUS!\n", status);
		return 0;
	}
	*/
	/*
	if (g_pIoSetDeviceInterfacePropertyData != NULL) {

		status = WdfStringCreate(NULL,
			WDF_NO_OBJECT_ATTRIBUTES,
			&symbolicLinkString);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
				"WdfStringCreate failed  %!STATUS!\n", status);
			return 0;
		}

		status = WdfDeviceRetrieveDeviceInterfaceString(device,
			(LPGUID)&GUID_DEVINTERFACE_OSRUSBFX2,
			NULL,
			symbolicLinkString);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceRetrieveDeviceInterfaceString failed  %!STATUS!\n", status);
			return 0;
		}

		WdfStringGetUnicodeString(symbolicLinkString, &symbolicLinkName);

		isRestricted = DEVPROP_TRUE;

		status = g_pIoSetDeviceInterfacePropertyData(&symbolicLinkName,
			&DEVPKEY_DeviceInterface_Restricted,
			0,
			0,
			DEVPROP_TYPE_BOOLEAN,
			sizeof(isRestricted),
			&isRestricted);

		WdfObjectDelete(symbolicLinkString);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "IoSetDeviceInterfacePropertyData failed  %!STATUS!\n", status);
			return 0;
		}
	}
	*/

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

	synccom_flist_init(&port->queued_oframes);
	synccom_flist_init(&port->sent_oframes);
	synccom_flist_init(&port->queued_iframes);
	synccom_flist_init(&port->pending_iframes);

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "%s: Exiting.", __FUNCTION__);
	return port;
}

NTSTATUS setup_spinlocks(_In_ struct synccom_port *port)
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

	status = WdfSpinLockCreate(&attributes, &port->sent_oframes_spinlock);
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

NTSTATUS setup_request(_In_ struct synccom_port *port)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, WdfUsbTargetPipeGetIoTarget(port->data_read_pipe), &port->data_read_request);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Unable to make ReadRequest! %!STATUS!", __FUNCTION__, status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS setup_memory(_In_ struct synccom_port *port)
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

NTSTATUS setup_dpc(_In_ struct synccom_port *port)
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

	WDF_DPC_CONFIG_INIT(&dpcConfig, &clear_oframe_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->clear_oframe_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: process_read_dpc failed %!STATUS!", __FUNCTION__, status);
		return status;
	}

	return STATUS_SUCCESS;
}

NTSTATUS SyncComEvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourceList, WDFCMRESLIST ResourceListTranslated)
{
    NTSTATUS                            status = STATUS_SUCCESS;
    WDF_USB_DEVICE_INFORMATION          deviceInfo;
    ULONG                               wait_wake_enable;
	struct synccom_memory_cap			memory_cap;
	unsigned char						clock_bits[20] = DEFAULT_CLOCK_BITS;
	struct synccom_port					*port = 0;
	WDF_USB_CONTINUOUS_READER_CONFIG	readerConfig;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();
	wait_wake_enable = FALSE;
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "%s: Entering.\n", __FUNCTION__);

	port = GetPortContext(Device);
	return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
    if (port->usb_device == NULL) {
        WDF_USB_DEVICE_CREATE_CONFIG config;

        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&config, USBD_CLIENT_CONTRACT_VERSION_602);

        status = WdfUsbTargetDeviceCreateWithParameters(Device, &config, WDF_NO_OBJECT_ATTRIBUTES, &port->usb_device);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfUsbTargetDeviceCreateWithParameters failed with Status code %!STATUS!\n", __FUNCTION__, status);
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
    else  {
        port->usb_traits = 0;
    }

    status = SelectInterfaces(port);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: SelectInterfaces failed 0x%x\n", __FUNCTION__, status);
        return status;
    }

	if (wait_wake_enable) {
        status = OsrFxSetPowerPolicy(port);
        if (!NT_SUCCESS (status)) {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: OsrFxSetPowerPolicy failed  %!STATUS!\n", __FUNCTION__, status);
            return status;
        }
    }

	WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&readerConfig, port_received_data, port, 512);
	readerConfig.NumPendingReads = 1;
	readerConfig.EvtUsbTargetPipeReadersFailed = FX3EvtReadFailed;
	status = WdfUsbTargetPipeConfigContinuousReader(port->data_read_pipe, &readerConfig);

	synccom_port_set_append_status(port, DEFAULT_APPEND_STATUS_VALUE);
	synccom_port_set_append_timestamp(port, DEFAULT_APPEND_TIMESTAMP_VALUE);
	synccom_port_set_ignore_timeout(port, DEFAULT_IGNORE_TIMEOUT_VALUE);
	synccom_port_set_tx_modifiers(port, DEFAULT_TX_MODIFIERS_VALUE);
	synccom_port_set_rx_multiple(port, DEFAULT_RX_MULTIPLE_VALUE);
	synccom_port_set_wait_on_write(port, DEFAULT_WAIT_ON_WRITE_VALUE);
	port->valid_frame_size = 0;

	memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
	memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;

	synccom_port_set_memory_cap(port, &memory_cap);

	port->pending_oframe = 0;

	SYNCCOM_REGISTERS_INIT(port->register_storage);
	synccom_port_set_clock_bits(port, clock_bits);

	port->register_storage.FIFOT = DEFAULT_FIFOT_VALUE;
	port->register_storage.CCR0 = DEFAULT_CCR0_VALUE;
	port->register_storage.CCR1 = DEFAULT_CCR1_VALUE;
	port->register_storage.CCR2 = DEFAULT_CCR2_VALUE;
	port->register_storage.BGR = DEFAULT_BGR_VALUE;
	port->register_storage.SSR = DEFAULT_SSR_VALUE;
	port->register_storage.SMR = DEFAULT_SMR_VALUE;
	port->register_storage.TSR = DEFAULT_TSR_VALUE;
	port->register_storage.TMR = DEFAULT_TMR_VALUE;
	port->register_storage.RAR = DEFAULT_RAR_VALUE;
	port->register_storage.RAMR = DEFAULT_RAMR_VALUE;
	port->register_storage.PPR = DEFAULT_PPR_VALUE;
	port->register_storage.TCR = DEFAULT_TCR_VALUE;
	port->register_storage.IMR = DEFAULT_IMR_VALUE;
	port->register_storage.DPLLR = DEFAULT_DPLLR_VALUE;
	port->register_storage.FCR = DEFAULT_FCR_VALUE;

	synccom_port_set_registers(port, &port->register_storage);

	synccom_port_purge_rx(port);
	synccom_port_purge_tx(port);

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_PNP, "%s: Exiting.\n", __FUNCTION__);
    return status;
}

NTSTATUS SyncComEvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct synccom_port *port = 0;
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "%s: Entering\n", __FUNCTION__);
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

	WdfSpinLockAcquire(port->sent_oframes_spinlock);
	synccom_flist_delete(&port->sent_oframes);
	WdfSpinLockRelease(port->sent_oframes_spinlock);

	return status;
}

NTSTATUS SyncComEvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
	NTSTATUS                status = STATUS_SUCCESS;
	struct synccom_port					*port = 0;

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
	struct synccom_port *port = 0;
	UNREFERENCED_PARAMETER(TargetState);
	PAGED_CODE();
	port = GetPortContext(Device);

	return_val_if_untrue(port, STATUS_SUCCESS);
	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(port->data_read_pipe), WdfIoTargetCancelSentIo);
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_POWER, "%s: Exiting.\n", __FUNCTION__);

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS OsrFxSetPowerPolicy(_In_ struct synccom_port *port)
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
    if ( !NT_SUCCESS(status)) {
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
NTSTATUS SelectInterfaces(_In_ struct synccom_port *port)
{
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    NTSTATUS                            status = STATUS_SUCCESS;
    WDFUSBPIPE                          pipe;
    WDF_USB_PIPE_INFORMATION            pipeInfo;
    UCHAR                               index;
    UCHAR                               numberConfiguredPipes;
    PAGED_CODE();


    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE( &configParams);

    status = WdfUsbTargetDeviceSelectConfig(port->usb_device, WDF_NO_OBJECT_ATTRIBUTES, &configParams);
    if(!NT_SUCCESS(status)) {

        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: WdfUsbTargetDeviceSelectConfig failed %!STATUS! \n", __FUNCTION__, status);
        return status;
    }

    port->usb_interface = configParams.Types.SingleInterface.ConfiguredUsbInterface;
    numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

    for(index=0; index < numberConfiguredPipes; index++) {
		
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

    if(!(port->data_read_pipe
         && port->data_write_pipe 
		 && port->register_read_pipe
		 && port->register_write_pipe )) {
        status = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "%s: Device is not CONFIGURED properly %!STATUS!\n", __FUNCTION__,status);
        return status;
    }
	
    return status;
}