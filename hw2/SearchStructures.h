// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "Pipe.h"

class bitmap {
	DWORD* table;
public:
	LONG n_discovered = 0;
	void init() {
		table = (DWORD*)calloc((((1ULL)<<32) >> 5), sizeof(DWORD));
	}
	bool interlocked_test_and_set(DWORD room_id) {
		DWORD offset = room_id >> 5; // room_id / 32
		DWORD bit = room_id & 0x1F; // room_id % 32
		DWORD mask = 1UL << bit;

		if ((table[offset] & mask) != 0) {
			return false;
		}

		DWORD previous_mask = InterlockedOr((LONG volatile*)(table + offset), mask);
		if ((previous_mask & mask) == 0) {
			InterlockedIncrement(&n_discovered);
			return true;
		}
		return false;
	}
	LONG read_nd() {
		return InterlockedAdd(&n_discovered, 0);
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
	~queue() {
		HeapDestroy(heap);
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
		memcpy(buffer + q_size, array, sizeof(DWORD) * n_items);
		q_size += n_items;
	};
	DWORD pop(DWORD* array, DWORD batch_size) {
		DWORD items_to_pop = min(batch_size, q_size);
		memcpy(array, buffer + q_size - items_to_pop, sizeof(DWORD) * items_to_pop);
		q_size -= items_to_pop;
		
		// deallocating if used space < half of capacity
		if (q_size < space_allocated / 2) {
			space_allocated /= 2;
			buffer = (DWORD*)HeapReAlloc(heap, HEAP_ZERO_MEMORY, buffer, sizeof(DWORD) * space_allocated);

			if (buffer == NULL) {
				printf("Error %d reallocating queue buffer\n", GetLastError());
				exit(-1);
			}
		}

		return items_to_pop;
	}
	DWORD size() {
		return q_size;
	}
	DWORD* get_buffer() {
		return buffer;
	}
	void reset_q_size() {
		q_size = 0;
	}
};