#include <Windows.h>
#include "synccom.h"

int main(void)
{
	HANDLE h = 0;
	DWORD tmp;
	unsigned status = 0;

	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);

	DeviceIoControl(h, SYNCCOM_GET_APPEND_TIMESTAMP,
		NULL, 0,
		&status, sizeof(status),
		&tmp, (LPOVERLAPPED)NULL);

	DeviceIoControl(h, SYNCCOM_ENABLE_APPEND_TIMESTAMP,
		NULL, 0,
		NULL, 0,
		&tmp, (LPOVERLAPPED)NULL);

	DeviceIoControl(h, SYNCCOM_DISABLE_APPEND_TIMESTAMP,
		NULL, 0,
		NULL, 0,
		&tmp, (LPOVERLAPPED)NULL);

	CloseHandle(h);

	return 0;
}