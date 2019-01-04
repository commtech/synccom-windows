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

#ifndef SYNCCOM_FLIST_H
#define SYNCCOM_FLIST_H

#include <ntddk.h>
#include <wdf.h>
#include <defines.h>

#include "trace.h"

void synccom_flist_init(struct synccom_flist *flist);
void synccom_flist_delete(struct synccom_flist *flist);
void synccom_flist_add_frame(struct synccom_flist *flist, struct synccom_frame *frame);
struct synccom_frame *synccom_flist_remove_frame(struct synccom_flist *flist);
struct synccom_frame *synccom_flist_remove_frame_if_lte(struct synccom_flist *flist, unsigned size);
void synccom_flist_clear(struct synccom_flist *flist);
BOOLEAN synccom_flist_is_empty(struct synccom_flist *flist);
unsigned synccom_flist_calculate_memory_usage(struct synccom_flist *flist);
struct synccom_frame *synccom_flist_peak_front(struct synccom_flist *flist);
struct synccom_frame *synccom_flist_peak_back(struct synccom_flist *flist);

#endif