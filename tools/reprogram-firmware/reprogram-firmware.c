#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "synccom.h"

// Just for reference, I'll keep this here.
#define CURRENT_FIRMWARE_FILE "sfscc2usb_Rev_0119_sync.s19"

int main(int argc, char *argv[])
{
	HANDLE h = 0;
	DWORD tmp;
	unsigned char firmware_line[50], handle_string[20];
	FILE *firmware_file;
	int i = 0;
	errno_t err;

	if (argc != 3)
	{
		printf("Usage:\n\t%s [port number] [firmware file]\n", argv[0]);
		return EXIT_FAILURE;
	}
	sprintf_s(handle_string, 20, "\\\\.\\SYNCCOM%d", atoi(argv[1]));
	printf("%s\n", handle_string);
	h = CreateFileA(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		CloseHandle(h);
		return EXIT_FAILURE;
	}
	err = fopen_s(&firmware_file, argv[2], "r");
	if (err != 0) {
		fprintf(stderr, "fopen failed file %s with: %d\n", argv[2], GetLastError());
		CloseHandle(h);
		return EXIT_FAILURE;
	}
	if (fgets(firmware_line, 50, firmware_file) == NULL)
	{
		fprintf(stderr, "failed to read line: %d\n", GetLastError());
		CloseHandle(h);
		fclose(firmware_file);
		return EXIT_FAILURE;
	}

	// A sleep is required here - there is no way to be certain that the erase completed, but a 5s sleep should be more
	// than adequate.
	// The first line of each program file is always an 'erase' command.
	printf("Erasing firmware...");
	DeviceIoControl(h, SYNCCOM_REPROGRAM, &firmware_line, sizeof(firmware_line), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	Sleep(5000);
	printf("Firmware erased!\n");
	printf("Programming..");
	
	// We can just write line by line - nothing really special is needed.
	while (1) {
		i++;
		if (fgets(firmware_line, 50, firmware_file) == NULL) break;
		DeviceIoControl(h, SYNCCOM_REPROGRAM, &firmware_line, sizeof(firmware_line), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
		if (i > 100000) {
			printf("File exceeded 100,000 lines. May not be correct! Programming failed, exiting..\n");
			break;
		}
	}
	Sleep(10000);
	// Unfortunately there's no way to verify success other than testing.
	printf("\n\nProgramming finished! Please power cycle the device and test it before using it.\n");

	fclose(firmware_file);
	CloseHandle(h);
	return EXIT_SUCCESS;
}