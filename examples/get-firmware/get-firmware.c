#include <windows.h>
#include <stdio.h>
#include "../../inc/synccom.h"

int main(void)
{
    HANDLE h = 0;
	DWORD tmp;
	UINT32 firmware_revision = 0;

	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);

	DeviceIoControl(h, SYNCCOM_GET_FIRMWARE,
		NULL, 0,
		&firmware_revision, sizeof(firmware_revision),
		&tmp, (LPOVERLAPPED)NULL);

	printf("Current firmware: 0x%8.8x\n", firmware_revision);
	CloseHandle(h);

	return 0;
}