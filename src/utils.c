/*
Copyright 2023 Commtech, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "utils.h"

UINT32 chars_to_u32(const unsigned char* data)
{
	return *((UINT32*)data);
}

unsigned is_read_only_register(unsigned offset)
{
	switch (offset) {
	case FIFO_BC_OFFSET:
	case FIFO_FC_OFFSET:
	case STAR_OFFSET:
	case VSTR_OFFSET:
	case ISR_OFFSET:
		return 1;
	}

	return 0;
}

unsigned is_write_only_register(unsigned offset)
{
	switch (offset) {
	case CMDR_OFFSET:
		return 1;
	}
	return 0;
}
