/*
Copyright 2019 Commtech, Inc.

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