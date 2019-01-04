/*
	Copyright (C) 2019  Commtech, Inc.

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

#ifndef SYNCCOM_UTILS_H
#define SYNCCOM_UTILS_H
#include <ntddk.h>
#include <wdf.h>
#include <defines.h>
#include "trace.h"

UINT32 chars_to_u32(const unsigned char *data);
unsigned is_read_only_register(unsigned offset);
unsigned is_write_only_register(unsigned offset);

#endif
