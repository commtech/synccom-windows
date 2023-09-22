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


#include "flist.h"
#include "utils.h" /* return_{val_}if_true */
#include "frame.h"


#if defined(EVENT_TRACING)
#include "flist.tmh"
#endif

void synccom_flist_init(struct synccom_flist *flist)
{
	flist->estimated_memory_usage = 0;

	InitializeListHead(&flist->frames);
}

void synccom_flist_delete(struct synccom_flist *flist)
{
	return_if_untrue(flist);

	synccom_flist_clear(flist);
}

void synccom_flist_add_frame(struct synccom_flist *flist, struct synccom_frame *frame)
{
	InsertTailList(&flist->frames, &frame->list);

	flist->estimated_memory_usage += synccom_frame_get_length(frame);
}

struct synccom_frame *synccom_flist_peek_front(struct synccom_flist *flist)
{
	if (IsListEmpty(&flist->frames))
		return 0;

	return CONTAINING_RECORD(flist->frames.Flink, synccom_FRAME, list);
}

struct synccom_frame *synccom_flist_peek_back(struct synccom_flist *flist)
{
	if (IsListEmpty(&flist->frames))
		return 0;
	
	return CONTAINING_RECORD(flist->frames.Blink, synccom_FRAME, list);
}

struct synccom_frame *synccom_flist_remove_frame(struct synccom_flist *flist)
{
	struct synccom_frame *frame = 0;

	if (IsListEmpty(&flist->frames))
		return 0;

	frame = CONTAINING_RECORD(flist->frames.Flink, synccom_FRAME, list);

	RemoveHeadList(&flist->frames);

	flist->estimated_memory_usage -= synccom_frame_get_length(frame);

	return frame;
}

struct synccom_frame *synccom_flist_remove_frame_if_lte(struct synccom_flist *flist, unsigned size)
{
	struct synccom_frame *frame = 0;
	unsigned frame_length = 0;

	if (IsListEmpty(&flist->frames))
		return 0;

	frame = CONTAINING_RECORD(flist->frames.Flink, synccom_FRAME, list);

	frame_length = synccom_frame_get_length(frame);

	if (frame_length > size)
		return 0;

	RemoveHeadList(&flist->frames);

	flist->estimated_memory_usage -= frame_length;

	return frame;
}

void synccom_flist_clear(struct synccom_flist *flist)
{
	while (!IsListEmpty(&flist->frames)) {
		LIST_ENTRY *frame_iter = 0;
		struct synccom_frame *frame = 0;

		frame_iter = RemoveHeadList(&flist->frames);
		frame = CONTAINING_RECORD(frame_iter, synccom_FRAME, list);

		synccom_frame_delete(frame);
	}

	flist->estimated_memory_usage = 0;
}

BOOLEAN synccom_flist_is_empty(struct synccom_flist *flist)
{
	return IsListEmpty(&flist->frames);
}

unsigned synccom_flist_calculate_memory_usage(struct synccom_flist *flist)
{
	LIST_ENTRY *frame_iter = 0;
	unsigned memory = 0;

	frame_iter = flist->frames.Flink;
	while (frame_iter != flist->frames.Blink) {
		struct synccom_frame *current_frame = 0;

		current_frame = CONTAINING_RECORD(frame_iter, synccom_FRAME, list);
		memory += synccom_frame_get_length(current_frame);

		frame_iter = frame_iter->Flink;
	}

	return memory;
}