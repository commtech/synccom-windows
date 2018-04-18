#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <time.h>
#include "../../inc/synccom.h"
#include "../../inc/calculate_clock_bits.h"

#define MAX_BUFFER_SIZE 5000

int set_port(HANDLE h, int mode, int clock_mode);
int purge(HANDLE h);
int set_clock(HANDLE h, int clock_speed);
int loop_test(HANDLE h, int max_size, int num_loops);

int main(int argc, char *argv[])
{
    HANDLE h = 0;
    unsigned char handle_string[20];
    int errors = 0, return_value;
    size_t num_loops = 0, max_size = 0;
    DWORD tmp;

    // Verify the user is providing enough information.
    if (argc != 4)
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
    if (max_size > MAX_BUFFER_SIZE) {
        printf("Supplied size is too large! Setting maximum write size to %d!", MAX_BUFFER_SIZE);
        max_size = MAX_BUFFER_SIZE;
    }
    DeviceIoControl(h, SYNCCOM_ENABLE_APPEND_STATUS, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
    srand(time(NULL));
    return_value = set_clock(h, 18432000);
    if (return_value != 1) printf("set clock: %d\n", return_value);
    return_value = set_port(h, 1, 1);
    if (return_value != 1) printf("set port: %d\n", return_value);
    return_value = purge(h);
    if (return_value != 1) printf("purge: %d\n", return_value);
    return_value = loop_test(h, max_size, num_loops);
    printf("loop_test: %d errors\n", return_value);

    return_value = set_clock(h, 50000000);
    if (return_value != 1) printf("set clock: %d\n", return_value);
    return_value = set_port(h, 1, 1);
    if (return_value != 1) printf("set port: %d\n", return_value);
    return_value = purge(h);
    if (return_value != 1) printf("purge: %d\n", return_value);
    return_value = loop_test(h, max_size, num_loops);
    printf("loop_test: %d errors\n", return_value);

    return_value = set_clock(h, 1000000);
    if (return_value != 1) printf("set clock: %d\n", return_value);
    return_value = set_port(h, 2, 1);
    if (return_value != 1) printf("set port: %d\n", return_value);
    return_value = purge(h);
    if (return_value != 1) printf("purge: %d\n", return_value);
    return_value = loop_test(h, max_size, num_loops);
    printf("loop_test: %d errors\n", return_value);

    return_value = set_clock(h, 1000000);
    if (return_value != 1) printf("set clock: %d\n", return_value);
    return_value = set_port(h, 3, 1);
    if (return_value != 1) printf("set port: %d\n", return_value);
    return_value = purge(h);
    if (return_value != 1) printf("purge: %d\n", return_value);
    return_value = loop_test(h, max_size, num_loops);
    printf("loop_test: %d errors\n", return_value);

    CloseHandle(h);

    return 0;
}

int set_port(HANDLE h, int mode, int clock_mode)
{
    struct synccom_registers regs;
    DWORD tmp;
    int ret_val;

    SYNCCOM_REGISTERS_INIT(regs);
    regs.CCR0 = 0x0011201c;
    DeviceIoControl(h, SYNCCOM_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
    Sleep(1000);
    switch (mode) {
    case 1:
        regs.CCR0 = 0x00112000;
        regs.CCR1 = 0x18;
        regs.TSR = regs.SSR = 0x7e;
        break;
    case 2:
        regs.CCR0 = 0x102;
        regs.CCR1 = 0xf00c;
        regs.TSR = regs.SSR = 0;
        break;
    case 3:
        regs.CCR0 = 0x00148001;
        regs.CCR1 = 0xc;
        regs.SSR = 0x74e331bb;
        regs.TSR = 0xbb12e3ff;
        break;
    }
    regs.CCR0 = regs.CCR0 | (clock_mode << 2);
    ret_val = DeviceIoControl(h, SYNCCOM_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);

    return ret_val;
}

int loop_test(HANDLE h, int max_size, int num_loops)
{
    int i = 0, j = 0, write_size, errors = 0;
    unsigned char odata[MAX_BUFFER_SIZE], idata[MAX_BUFFER_SIZE + 2];
    DWORD bytes_written, bytes_read;

    Sleep(1000);
    for (i = 0; i < (int)num_loops; i++)
    {
        // Generate random data of a random size.
        write_size = (rand() % max_size)+1;
        for (j = 0; j < write_size; j++) odata[j] = rand() % 255;

        // Now we write and verify we wrote the correct amount.
        WriteFile(h, odata, write_size, &bytes_written, NULL);
        if (bytes_written != write_size) printf("%d: Failed to write the total frame! Attmpted to write %d, wrote %d.\n", i, write_size, bytes_written);

        // Read the data and verify that it's correct.
        ReadFile(h, idata, sizeof(idata), &bytes_read, NULL);
        if (bytes_written + 2 != bytes_read) {
            printf("%d: Didn't read the correct number of bytes! Wrote %d bytes, read %d bytes!\n", i, bytes_written, bytes_read);
            errors++;
        }
        for (j = 0; j < (int)bytes_written; j++) if (odata[j] != idata[j]) {
            printf("%d: Data mismatch: byte %d: 0x%2.2x != 0x%2.2x\n", i, j, odata[j], idata[j]);
            errors++;
        }
        if (idata[bytes_read - 2] != 0x04 || idata[bytes_read - 1] != 0x00) {
            printf("0x%2.2x 0x%2.2x\n", idata[bytes_read - 2], idata[bytes_read - 1]);
            errors++;
        }

        printf("Loop: %d\r", i);
        // Once we have an error, we break.
        if (errors) break;
    }
    return errors;
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

int set_clock(HANDLE h, int clock_speed)
{
    unsigned char clock_bits[20];
    DWORD tmp;
    int ret_val;

    calculate_clock_bits(clock_speed, 10, clock_bits);
    ret_val = DeviceIoControl(h, SYNCCOM_SET_CLOCK_BITS, &clock_bits, sizeof(clock_bits), NULL, 0, &tmp, (LPOVERLAPPED)NULL);

    return ret_val;
}