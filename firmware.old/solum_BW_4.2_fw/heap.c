#include <stdio.h>
#include "heap.h"
#include "fw.h"



struct HeapNode {
	
	struct HeapNode *prev;
	uint32_t used	: 1;
	uint32_t size	:31;
	uint8_t data[];
};

struct Heap {
	
	struct HeapNode* head;
	struct HeapNode* tail;
};

static struct Heap *heapPrvGet(void)
{
	return (struct Heap*)gHeapMemory;
}

static struct HeapNode* heapPrvGetNext(struct Heap* h, struct HeapNode* node)
{
	if (h->tail == node)
		return NULL;
	else
		return (struct HeapNode*)(node->data + node->size);
}

void heapInit(void)
{
	uint32_t size = MZ_AVAIL_HEAP_MEM;
	struct Heap *h = heapPrvGet();
	struct HeapNode *node;
	
	size = size &~ 3;
	node = (struct HeapNode*)(h + 1);
	
	h->head = node;
	h->tail = node;
	
	node->used = 0;
	node->prev = NULL;
	node->size = size - sizeof(struct HeapNode) - sizeof(struct Heap);
}

void* heapAlloc(uint32_t sz)
{
	struct HeapNode *node, *best = NULL;
	struct Heap *h = heapPrvGet();
	void *ret = NULL;
	
	node = h->head;
	sz = (sz + 3) &~ 3;
	
	for (node = h->head; node; node = heapPrvGetNext(h, node)) {
	
		if (!node->used && node->size >= sz && (!best || best->size > node->size))
			best = node;
	}
	
	if (!best)
		return NULL;
	
	if (best->size - sz > sizeof(struct HeapNode)) {		//there is a point to split up the chunk
		
		node = (struct HeapNode*)(best->data + sz);
		
		node->used = 0;
		node->size = best->size - sz - sizeof(struct HeapNode);
		node->prev = best;
		
		if (best != h->tail)
			heapPrvGetNext(h,node)->prev = node;
		else
			h->tail = node;
		
		best->size = sz;
	}
	
	best->used = 1;
	
	return best->data;
}

void heapFree(void* ptr)
{
	struct Heap *h = heapPrvGet();
	struct HeapNode *node, *t;
	
	node = ((struct HeapNode*)ptr) - 1;
	node->used = 0;
	
	//merge back
	while (node->prev && !node->prev->used)
		node = node->prev;
	
	while ((t = heapPrvGetNext(h,node)) && !t->used) {
		
		node->size += sizeof(struct HeapNode) + t->size;
		if (h->tail == t)
			h->tail = node;
	}
	
	if ((t = heapPrvGetNext(h,node)) != NULL)
		t->prev = node;
}

