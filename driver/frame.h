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

#ifndef SYNCCOM_FRAME_H
#define SYNCCOM_FRAME_H

//#include "descriptor.h" /* struct synccom_descriptor */
#include <ntddk.h>
#include <wdf.h>
#include <defines.h>


struct synccom_frame *synccom_frame_new(struct synccom_port *port);
void synccom_frame_delete(struct synccom_frame *frame);

int synccom_frame_update_buffer_size(struct synccom_frame *frame, unsigned size);
unsigned synccom_frame_get_length(struct synccom_frame *frame);
unsigned synccom_frame_get_buffer_size(struct synccom_frame *frame);
unsigned synccom_frame_get_frame_size(struct synccom_frame *frame);
int synccom_frame_add_data(struct synccom_frame *frame, const unsigned char *data, unsigned length);
int synccom_frame_copy_data(struct synccom_frame *destination, struct synccom_frame *source);
int synccom_frame_remove_data(struct synccom_frame *frame, char unsigned *destination, unsigned length);
unsigned synccom_frame_is_empty(struct synccom_frame *frame);
void synccom_frame_clear(struct synccom_frame *frame);


#endif