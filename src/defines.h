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

#ifndef SYNCCOM_DEFINES_H
#define SYNCCOM_DEFINES_H

#include "precomp.h"
#include "synccom.h"

// {1F67CDC8-3E4C-42C6-980C-A79E79C728BC}
static const GUID SYNCCOM_GUID = { 0x1f67cdc8, 0x3e4c, 0x42c6, { 0x98, 0xc, 0xa7, 0x9e, 0x79, 0xc7, 0x28, 0xbc } };
// {72896E8A-1CAC-4451-B295-255D3853C1F5}
DEFINE_GUID(GUID_DEVCLASS_SYNCCOM, 0x72896e8a, 0x1cac, 0x4451, 0xb2, 0x95, 0x25, 0x5d, 0x38, 0x53, 0xc1, 0xf5);
// {573E8C73-0CB4-4471-A1BF-FAB26C31D384}
DEFINE_GUID(GUID_DEVINTERFACE_SYNCCOM, 0x36fc9e60, 0xc465, 0x11cf, 0x80, 0x56, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
// {DCCFD794-CB51-4D3B-8907-664F179EABA6}
//DEFINE_GUID(GUID_DEVINTERFACE_SYNCCOM, 0xdccfd794, 0xcb51, 0x4d3b, 0x89, 0x7, 0x66, 0x4f, 0x17, 0x9e, 0xab, 0xa6);

#define DEVICE_NAME "synccom"

#define REGISTER_WRITE_ENDPOINT 0x01
#define REGISTER_READ_ENDPOINT 0x81
#define DATA_WRITE_ENDPOINT 0x06
#define DATA_READ_ENDPOINT 0x82

// These are defined in the firmware, so shouldn't be changed unless you are 100% sure
// you know what you are doing.
#define SYNCCOM_READ_FX2_FIRMWARE            0x02 // write: 0x02
												 // read : (value - 4B)
#define SYNCCOM_WRITE_REGISTER				0x6A // write: 0x6A (address - 2B) (value - 4B)
#define SYNCCOM_READ_REGISTER				0x6B // write: 0x6B (address - 2B)
												 // read : (value - 4B)
#define SYNCCOM_READ_WITH_ADDRESS			0x6C // write: 0x6C (address - 2B)
												 // read : (address - 2B) (value - 4B)
#define SYNCCOM_READ_WAIT_HIGH_VAL  		0x6D // write: 0x6D (address - 2B) (timeout - 1B) (mask - 4B)
												 // read : (address - 2B) (value - 4B)
#define SYNCCOM_WRITE_NONVOLATILE           0x6E // write: 0x6E (value - 4B)
#define SYNCCOM_READ_NONVOLATILE            0x6F // write: 0x6F
												 // read : (value - 4B)

#define SYNCCOM_GET_STATUS					0xFE

#define TX_FIFO_SIZE 4096
#define EXTRA_BLOCK_SIZE 16384
#define TIMER_DELAY_MS 250
#define MAXIMUM_WRITES_IN_FLIGHT 20

#define UNUSED(x) (void)(x)

#define warn_if_untrue(expr) if (expr) {} else  {  KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue."));  }
#define return_if_untrue(expr)  if (expr) {} else {  KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue."));  return;  }
#define return_val_if_untrue(expr, val)  if (expr) {} else  {  KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue."));  return val;  }
#define _BYTESWAP_UINT32(value) ((value << 24) | ((value << 8) & 0x00ff0000) | ((value >> 8) & 0x0000ff00) | (value >> 24))

#define SYNCCOM_UPPER_OFFSET 0x80
#define FPGA_UPPER_ADDRESS 0x00

#define FIFO_OFFSET 0x00
#define BC_FIFO_L_OFFSET 0x04
#define FIFOT_OFFSET 0x08
#define FIFO_BC_OFFSET 0x0C
#define FIFO_FC_OFFSET 0x10
#define CMDR_OFFSET 0x14
#define STAR_OFFSET 0x18
#define CCR0_OFFSET 0x1C
#define CCR1_OFFSET 0x20
#define CCR2_OFFSET 0x24
#define BGR_OFFSET 0x28
#define SSR_OFFSET 0x2C
#define SMR_OFFSET 0x30
#define TSR_OFFSET 0x34
#define TMR_OFFSET 0x38
#define RAR_OFFSET 0x3C
#define RAMR_OFFSET 0x40
#define PPR_OFFSET 0x44
#define TCR_OFFSET 0x48
#define VSTR_OFFSET 0x4C
#define ISR_OFFSET 0x50
#define IMR_OFFSET 0x54
#define DPLLR_OFFSET 0x58
#define MAX_OFFSET 0x58 //must equal the highest FCore register address

// Should only use these with the upper address of 0x00
#define FCR_OFFSET 0x20 
#define CONTROL_OFFSET 0x02

#define FLUSH_COMMAND 0x00000010


#define RFE 0x00000004
#define RFT 0x00000002
#define RFS 0x00000001
#define RFO 0x00000008
#define RDO 0x00000010
#define RFL 0x00000020
#define TIN 0x00000100
#define TDU 0x00040000
#define TFT 0x00010000
#define ALLS 0x00020000
#define CTSS 0x01000000
#define DSRC 0x02000000
#define CDC 0x04000000
#define CTSA 0x08000000
#define DR_STOP 0x00004000
#define DT_STOP 0x00008000
#define DT_FE 0x00002000
#define DR_FE 0x00001000
#define DT_HI 0x00000800
#define DR_HI 0x00000400

#define CE_BIT 0x00040000

typedef struct synccom_frame {
	LIST_ENTRY list;
	unsigned char* buffer;
	unsigned buffer_size;
	unsigned data_length;
	unsigned frame_size;
	unsigned number;
	unsigned lost_bytes;
	LARGE_INTEGER timestamp;
	struct synccom_port* port;
} synccom_FRAME;

struct synccom_flist {
	LIST_ENTRY frames;
	unsigned estimated_memory_usage;
};

typedef struct synccom_port {
	WDFUSBDEVICE                    usb_device;
	ULONG							usb_traits;
	WDFDEVICE						device;
	WDFUSBINTERFACE                 usb_interface;
	WDFUSBPIPE                      data_read_pipe;
	WDFUSBPIPE                      data_write_pipe;
	WDFUSBPIPE						register_write_pipe;
	WDFUSBPIPE						register_read_pipe;
	UINT32                          fx2_firmware_rev;
	UINT32							fpga_firmware_rev;
	UINT32                          nonvolatile_reg;

	unsigned port_number;
	struct synccom_registers register_storage;
	BOOLEAN append_status;
	BOOLEAN append_timestamp;
	BOOLEAN ignore_timeout;
	BOOLEAN rx_multiple;
	int pending_frame_size_reads;
	int tx_modifiers;
	int writes_in_flight;
	struct synccom_memory_cap memory_cap;

	WDFQUEUE write_queue;
	WDFQUEUE write_queue2; // TODO: Change name to be more descriptive. 
	WDFQUEUE read_queue;
	WDFQUEUE read_queue2; // TODO: Change name to be more descriptive. 
	WDFQUEUE ioctl_queue;

	WDFSPINLOCK board_settings_spinlock; // Anything that will alter the settings at a board level 
	WDFSPINLOCK board_rx_spinlock; // Anything that will alter the state of rx at a board level 
	WDFSPINLOCK board_tx_spinlock; // Anything that will alter the state of rx at a board level 
	WDFSPINLOCK request_spinlock;
	WDFSPINLOCK read_count_spinlock;
	WDFSPINLOCK frame_size_spinlock;

	WDFSPINLOCK queued_iframes_spinlock;
	struct synccom_flist queued_iframes; // Frames already retrieved from the FIFO 

	WDFSPINLOCK queued_oframes_spinlock;
	struct synccom_flist queued_oframes; // Frames not yet in the FIFO yet 

	WDFSPINLOCK istream_spinlock;
	struct synccom_frame* istream; // Transparent stream 

	WDFSPINLOCK pending_iframes_spinlock;
	struct synccom_flist pending_iframes; // Frames that are partially complete

	WDFSPINLOCK pending_oframe_spinlock;
	struct synccom_frame* pending_oframe; // Frame being put in the FIFO 

	WDFDPC oframe_dpc;
	WDFDPC iframe_dpc;
	WDFDPC process_read_dpc;

	WDFTIMER timer;

	WDFREQUEST data_read_request;
	WDFMEMORY  data_read_memory;

} SYNCCOM_PORT, * PSYNCCOM_PORT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SYNCCOM_PORT, GetPortContext)

#define DEFAULT_INPUT_MEMORY_CAP_VALUE 1000000
#define DEFAULT_OUTPUT_MEMORY_CAP_VALUE 1000000

#define DEFAULT_HOT_PLUG_VALUE 0
#define DEFAULT_FORCE_FIFO_VALUE 1
#define DEFAULT_APPEND_STATUS_VALUE 0
#define DEFAULT_APPEND_TIMESTAMP_VALUE 0
#define DEFAULT_IGNORE_TIMEOUT_VALUE 0
#define DEFAULT_TX_MODIFIERS_VALUE XF
#define DEFAULT_RX_MULTIPLE_VALUE 0

#define DEFAULT_FIFOT_VALUE 0x00040004
#define DEFAULT_CCR0_VALUE 0x0011201c
#define DEFAULT_CCR1_VALUE 0x00000018
#define DEFAULT_CCR2_VALUE 0x00000000
#define DEFAULT_BGR_VALUE 0x00000000
#define DEFAULT_SSR_VALUE 0x0000007e
#define DEFAULT_SMR_VALUE 0x00000000
#define DEFAULT_TSR_VALUE 0x0000007e
#define DEFAULT_TMR_VALUE 0x00000000
#define DEFAULT_RAR_VALUE 0x00000000
#define DEFAULT_RAMR_VALUE 0x00000000
#define DEFAULT_PPR_VALUE 0x00000000
#define DEFAULT_TCR_VALUE 0x00000000
#define DEFAULT_IMR_VALUE 0x0f000000
#define DEFAULT_DPLLR_VALUE 0x00000004
#define DEFAULT_FCR_VALUE 0x00000000

/* 18.432 MHz */
#define DEFAULT_CLOCK_BITS {0x0f, 0x61, 0xe5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x16, 0x40, 0x01, 0x04, 0x00, 0xff, 0xff, 0xff } 

#endif