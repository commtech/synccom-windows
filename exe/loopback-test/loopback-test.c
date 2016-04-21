#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "../../inc/synccom.h"
#include "../../inc/calculate_clock_bits.h"

int main(void)
{
	HANDLE h = 0;
	DWORD bytes_written, bytes_read, tmp;
	unsigned char odata[5000], idata[5000];
	struct synccom_registers regs;
	int num_loops = 50, write_size = 100, errors = 0, j, i;
	unsigned char clock_bits[20];

	/* Open port 0 in a blocking IO mode */
	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		return EXIT_FAILURE;
	}
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
	calculate_clock_bits(6000000, 10, clock_bits);
	DeviceIoControl(h, SYNCCOM_SET_CLOCK_BITS, &clock_bits, sizeof(clock_bits), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	DeviceIoControl(h, SYNCCOM_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	DeviceIoControl(h, SYNCCOM_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	printf("Testing! Please wait.\n\n");
	// Currently because of the nature of USB you cannot be sure when the PURGE_TX and PURGE_RX has completed.
	// This seems adequate in making sure they complete.
	Sleep(1000);
	for (i = 0; i < num_loops; i++)
	{
		for (j = 0; j < write_size; j++) odata[j] = rand() % 255;
		WriteFile(h, odata, write_size, &bytes_written, NULL);
		ReadFile(h, idata, sizeof(idata), &bytes_read, NULL);
		if (bytes_written != bytes_read) {
			printf("%d: Didn't read the correct number of bytes! Wrote %d bytes, read %d bytes!\n", i, bytes_written, bytes_read);
			errors++;
		}
		for (j = 0; j < (int)bytes_read; j++) if (odata[j] != idata[j]) {
			printf("%d: Data mismatch: byte %d: 0x%2.2x != 0x%2.2x\n", i, j, odata[j], idata[j]);
			errors++;
		}
		if (errors) break;
	}

	printf("Test complete! %d errors!\n", errors);

	CloseHandle(h);

	return 0;
}