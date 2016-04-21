#include <Windows.h>
#include "../../inc/synccom.h"

int main(void)
{
	HANDLE h = 0;
	DWORD tmp;
	struct synccom_memory_cap memcap;

	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);

	memcap.input = 1000000; /* 1 MB */
	memcap.output = 1000000; /* 1 MB */

	DeviceIoControl(h, SYNCCOM_SET_MEMORY_CAP,
		&memcap, sizeof(memcap),
		NULL, 0,
		&tmp, (LPOVERLAPPED)NULL);

	CloseHandle(h);

	return 0;
}