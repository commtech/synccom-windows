#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "devioctl.h"
#include "../../inc/synccom.h"

int main()
{
	HANDLE h;
	struct synccom_registers regs;
	DWORD tmp;

	h = CreateFile(L"\\\\.\\SYNCCOM0", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		return EXIT_FAILURE;
	}
	SYNCCOM_REGISTERS_INIT(regs);

	/* Change the CCR0 and BGR elements to our desired values */
	regs.CCR0 = 0x0011201c;
	regs.BGR = 0;

	/* Set the CCR0 and BGR register values */
	DeviceIoControl(h, SYNCCOM_SET_REGISTERS,
		&regs, sizeof(regs),
		NULL, 0,
		&tmp, (LPOVERLAPPED)NULL);

	/* Re-initialize our registers structure */
	SYNCCOM_REGISTERS_INIT(regs);

	/* Mark the CCR0 and CCR1 elements to retrieve values */
	regs.CCR1 = SYNCCOM_UPDATE_VALUE;
	regs.CCR2 = SYNCCOM_UPDATE_VALUE;

	/* Get the CCR1 and CCR2 register values */
	DeviceIoControl(h, SYNCCOM_GET_REGISTERS, &regs, sizeof(regs), &regs, sizeof(regs), &tmp, (LPOVERLAPPED)NULL);

	CloseHandle(h);
	return 0;
}

