#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "../../inc/synccom.h"
#include "../../inc/calculate_clock_bits.h"

#define MAX_BUFFER_SIZE 5000

int main(int argc, char *argv[])
{
	HANDLE h = 0;
	DWORD bytes_written, bytes_read, tmp;
	unsigned char odata[MAX_BUFFER_SIZE], idata[MAX_BUFFER_SIZE+10], handle_string[20], clock_bits[20];
	struct synccom_registers regs;
	int write_size = 0, errors = 0, j, i;
	size_t num_loops= 0, max_size= 0;

	// Verify the user is providing enough information.
	if(argc != 4)
	{
		printf("Usage:\n\t%s [port number] [number of loops] [max size]\n", argv[0]);
		return EXIT_FAILURE;
	}
	// Open the port for read/write.
	sprintf_s(handle_string, 20, "\\\\.\\SYNCCOM%d", atoi(argv[1]));
	printf("%s\n", handle_string);
	h = CreateFileA(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		CloseHandle(h);
		return EXIT_FAILURE;
	}
	num_loops = atoi(argv[2]);
	max_size = atoi(argv[3]);
	if (max_size > sizeof(odata)) {
		printf("Supplied size is too large! Setting maximum write size to %d!", sizeof(odata));
		max_size = sizeof(odata);
	}

	// These are standard HDLC settings.
	SYNCCOM_REGISTERS_INIT(regs);
	regs.FIFOT = 0x00040004;
	regs.CCR0 = 0x00112004;
	regs.CCR1 = 0x00000018;
	regs.CCR2 = 0x00000000;
	regs.BGR = 0x00000000;
	regs.SSR = 0x0000007e;
	regs.SMR = 0x00000000;
	regs.TSR = 0x0000007e;
	regs.TMR = 0x00000000;
	regs.RAR = 0x00000000;
	regs.RAMR = 0x00000000;
	regs.PPR = 0x00000000;
	regs.TCR = 0x00000000;
	regs.IMR = 0x0f000000;
	regs.DPLLR = 0x00000004;
	DeviceIoControl(h, SYNCCOM_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);

	// Because the BGR register is 0, the clock is set to this value; 6,000,000.
	calculate_clock_bits(6000000, 10, clock_bits);
	DeviceIoControl(h, SYNCCOM_SET_CLOCK_BITS, &clock_bits, sizeof(clock_bits), NULL, 0, &tmp, (LPOVERLAPPED)NULL);

	// Always purge after potentially changing the 'mode' bits in the CCR0 register.
	DeviceIoControl(h, SYNCCOM_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	DeviceIoControl(h, SYNCCOM_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	
	// Currently because of the nature of USB you cannot be sure when the PURGE_TX and PURGE_RX has completed.
	// There is no return value for these commands, so we can only try and hope for the best.
	// This seems adequate in making sure they complete.
	Sleep(1000);

	printf("Testing %d loops! Please wait.\n\n", num_loops);
	for (i = 0; i < (int)num_loops; i++)
	{
		// Generate random data of a random size.
		write_size = rand() % max_size;
		for (j = 0; j < write_size; j++) odata[j] = rand() % 255;
		
		// Now we write and verify we wrote the correct amount.
		WriteFile(h, odata, write_size, &bytes_written, NULL);
		if (bytes_written != write_size) printf("%d: Failed to write the total frame! Attmpted to write %d, wrote %d.\n", write_size, bytes_written);

		// Read the data and verify that it's correct.
		ReadFile(h, idata, sizeof(idata), &bytes_read, NULL);
		if (bytes_written != bytes_read) {
			printf("%d: Didn't read the correct number of bytes! Wrote %d bytes, read %d bytes!\n", i, bytes_written, bytes_read);
			errors++;
		}
		for (j = 0; j < (int)bytes_read; j++) if (odata[j] != idata[j]) {
			printf("%d: Data mismatch: byte %d: 0x%2.2x != 0x%2.2x\n", i, j, odata[j], idata[j]);
			errors++;
		}

		// Once we have an error, we break.
		if (errors) break;
	}

	// Always close your handle when you're done using the port.
	printf("Test complete! %d errors!\n", errors);
	CloseHandle(h);

	return 0;
}