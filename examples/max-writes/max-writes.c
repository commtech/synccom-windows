#include <Windows.h>
#include "synccom.h"

/*
This is a simple example showing how to get and set the maximum pending writes to the Sync Com.
*/

int main(void)
{
	HANDLE h = 0;
	unsigned max_writes;
	DWORD tmp;

	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, 0, NULL);


	/* Get the maximum number of pending writes */
	DeviceIoControl(h, SYNCCOM_GET_TX_MODIFIERS,
		NULL, 0,
		&max_writes, sizeof(max_writes),
		&tmp, (LPOVERLAPPED)NULL);
		
	/* Set the maximum number of pending writes */
	max_writes = 30;
	DeviceIoControl(h, SYNCCOM_SET_MAX_WRITES,
		&max_writes, sizeof(max_writes),
		NULL, 0,
		&tmp, (LPOVERLAPPED)NULL);

	CloseHandle(h);

	return 0;
}