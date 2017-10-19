#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>
#include "../../inc/synccom.h"

#define MAX_BUFFER_SIZE 5000

int set_port(HANDLE h, int mode, int clock_mode);
int purge(HANDLE h);
int set_clock(HANDLE h, int clock_speed);
int loop_test(HANDLE h, int max_size, int num_loops);

int main(int argc, char *argv[])
{
    HANDLE h = 0;
    unsigned char handle_string[20];
    int return_value, port_num = 0;

	printf("\nThis program will turn the red and green lights on for a specific port, allowing you to identify which device that port is.\n\n");
	printf("Please enter a port number to turn the lights on : ");
	scanf_s("%d", &port_num);

    sprintf_s(handle_string, 20, "\\\\.\\SYNCCOM%d", port_num);
    h = CreateFileA(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "\n\nCould not open %s!\nGetLastError(): %d\n", handle_string, GetLastError());
        CloseHandle(h);
        return EXIT_FAILURE;
    }

    return_value = set_port(h, 1, 1);
    return_value = purge(h);
	printf("\n\nFind the device with the red and green lights on. This is the port you're looking for.\n");
	printf("\nPress any key to continue.");
	getchar(); 
	getchar();
	return_value = set_port(h, 2, 2);
	return_value = purge(h);

    CloseHandle(h);

    return 0;
}

int set_port(HANDLE h, int mode, int clock_mode)
{
    struct synccom_registers regs;
    DWORD tmp;
    int ret_val;

	UNREFERENCED_PARAMETER(clock_mode);
    SYNCCOM_REGISTERS_INIT(regs);
    regs.CCR0 = 0x0011201c;
    DeviceIoControl(h, SYNCCOM_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
    Sleep(1000);
    switch (mode) {
    case 1:
        regs.CCR0 = 0x0011209c;
        regs.CCR1 = 0x18;
        regs.TSR = regs.SSR = 0x7e;
        break;
    default:
		regs.CCR0 = 0x0011201c;
		regs.CCR1 = 0x18;
		regs.TSR = regs.SSR = 0x7e;
        break;
    }
    ret_val = DeviceIoControl(h, SYNCCOM_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);

    return ret_val;
}

int purge(HANDLE h)
{
    DWORD tmp;
    int return_value;

    Sleep(1000);
    return_value = DeviceIoControl(h, SYNCCOM_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
    return_value &= DeviceIoControl(h, SYNCCOM_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
    Sleep(1000);
    return return_value;
}
