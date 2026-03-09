// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "Pipe.h"

class bitmap {
	DWORD* table;
	LONG n_discovered = 0;
	int planet_num;
public:
	void init(int planet_num) {
		this->planet_num = planet_num;
		table = (DWORD*)calloc((((1ULL)<<planet_num) >> 5), sizeof(DWORD));
	}
	bool interlocked_test_and_set(DWORD room_id) {
		room_id = room_id >> (32 - planet_num);
		DWORD offset = room_id >> 5; // room_id / 32
		DWORD bit = room_id & 0x1F; // room_id % 32
		DWORD mask = 1UL << bit;
		DWORD previous_mask = InterlockedOr((LONG volatile*)(table + offset), mask);
		if ((previous_mask & mask) == 0) {
			n_discovered++;
			return true;
		}
		return false;
	}
	LONG read_nd() {
		return InterlockedExchangeAdd(&n_discovered, 0);
	}
};

class queue {
	HANDLE heap;
	DWORD* buffer;
	DWORD head, tail;
	DWORD space_allocated;
	DWORD q_size;
public:
	queue() {
		// initializing queue with space for 10000 items
		heap = HeapCreate(HEAP_NO_SERIALIZE, sizeof(DWORD)*10000, 0);
		buffer = (DWORD*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(DWORD) * 10000);
		head = 0;
		tail = 0;
		space_allocated = 10000;
		q_size = 0;
	}
	void push(DWORD* array, DWORD n_items) {
		// check if enough space allocated
		if (q_size + n_items > space_allocated) {
			// double space until enough
			while (q_size + n_items > space_allocated) {
				space_allocated *= 2;
			}
			buffer = (DWORD*)HeapReAlloc(heap, HEAP_ZERO_MEMORY, buffer, sizeof(DWORD) * space_allocated);

			if (buffer == NULL) {
				printf("Error %d reallocating queue buffer\n", GetLastError());
				exit(-1);
			}
		}
		memcpy(buffer + tail, array, sizeof(DWORD) * n_items);
		tail += n_items;
		q_size += n_items;
	};
	DWORD pop(DWORD*& array, DWORD batch_size) {
		DWORD items_to_pop = min(batch_size, q_size);
		array = (DWORD*)malloc(sizeof(DWORD) * items_to_pop);
		memcpy(array, buffer, sizeof(DWORD) * items_to_pop);
		memmove(buffer, buffer + items_to_pop, sizeof(DWORD) * (q_size - items_to_pop));
		q_size -= items_to_pop;
		tail -= items_to_pop;
		return items_to_pop;
	}
	DWORD size() {
		return q_size;
	}
	DWORD* get_buffer() {
		return buffer;
	}
};