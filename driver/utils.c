/*
	Copyright (C) 2016  Commtech, Inc.

	This file is part of synccom-windows.

	synccom-windows is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published bythe Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	synccom-windows is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along
	with synccom-windows.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <utils.h>

UINT32 chars_to_u32(const unsigned char *data)
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
	switch (offset){
	case CMDR_OFFSET:
		return 1;
	}
	return 0;
}
