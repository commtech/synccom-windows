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

#ifndef SYNCCOM_FRAME_H
#define SYNCCOM_FRAME_H

#include "precomp.h"
#include "defines.h"


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