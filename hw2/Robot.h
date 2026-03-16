// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "Pipe.h"

void format_with_commas(char*& buffer, DWORD num) {
	// special case of 0
	if (num == 0) {
		buffer = (char*)malloc(2);
		buffer[0] = '0';
		buffer[1] = '\0';
		return;
	}

	// count digits to find 
	DWORD temp = num;
	int n_digits = 0;
	while (temp > 0) {
		temp /= 10;
		++n_digits;
	}

	// initializing buffer to store
	int len = n_digits + ((n_digits - 1) / 3)+1;
	buffer = (char*)malloc(len);
	buffer[len - 1] = '\0';

	// filling in digits
	int pos = len - 2;
	int digit_cnt = 0;
	while (num > 0) {
		if (digit_cnt > 0 && digit_cnt % 3 == 0) {
			buffer[pos--] = ',';
		}

		buffer[pos--] = (num % 10) + '0'; // converting to ascii
		num /= 10;
		++digit_cnt;
	}
}

class Robot {
public:
	static DWORD WINAPI search(LPVOID lpParam) {
		// get params
		robot_init* init_data = (robot_init*)lpParam;

		// creating pipe
		int robot_i = init_data->robot_i;
		Pipe pipe{};
		pipe.create_pipe(init_data->pi, false, robot_i);

		// connecting to robot
		pipe.write(CONNECT, 0, 0, 0, 0, 0, robot_i);
		pipe.read();
		DWORD first_room = interpret_connection(pipe.buffer);

		// adding first room
		if (init_data->d->interlocked_test_and_set(first_room)) {
			WaitForSingleObject(*init_data->mutex, INFINITE);
			init_data->q[*init_data->qc].push(&first_room, 1);
			//*init_data->q_change_ready = true;
			ReleaseMutex(*init_data->mutex);
			ReleaseSemaphore(*init_data->sema, 1, NULL);
		}

		// creating waiter handles
		HANDLE wait_handles[2] = { *init_data->end, *init_data->sema };

		// calculating halfway discovered num
		DWORD halfway_discovered = (1ULL << init_data->planet) >> 1;

		// var to store how many new neighbors
		DWORD n_new_neighbors;
		LONG64 n_total_neighbors;

		// store rooms to explore
		DWORD* rooms = (DWORD*)malloc(sizeof(DWORD) * 10000);
		
		// storing level & n_processed
		int level = 0;
		LONG64 n_rooms_processed = 0;


		while (true) {
			// check if done or can proceed
			if (WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE) == WAIT_OBJECT_0) {
				break;
			}

			// popping batch of room to explore
			WaitForSingleObject(*init_data->mutex, INFINITE);
			DWORD n_popped = init_data->q[*init_data->qc].pop(rooms, MAX_BATCH);
			*init_data->working += 1;
			level = *init_data->level;
			n_rooms_processed = *init_data->n_rooms_processed;
			ReleaseMutex(*init_data->mutex);

			// adding to n_rooms_processed
			InterlockedAdd64((LONG64 volatile*)init_data->n_rooms_processed, n_popped);

			// exploring rooms
			pipe.write(MOVE, 0, 0, 0, rooms, n_popped, robot_i);
			pipe.read();
			
			n_new_neighbors = 0;
			n_total_neighbors = 0;
			interpret_move(n_new_neighbors, pipe.buffer, pipe.last_bytes_read, *init_data->d, robot_i, level, rooms, n_rooms_processed, n_total_neighbors);
			InterlockedAdd64((LONG64 volatile*)init_data->n_total_returns, n_total_neighbors);
			InterlockedAdd64((LONG64 volatile*)init_data->n_unique_returns, n_new_neighbors);
			
			// swapping qc and qf
			WaitForSingleObject(*init_data->mutex, INFINITE);
			if (n_new_neighbors > 0) {
				// adding new rooms to qf
				init_data->q[*init_data->qf].push((DWORD*)pipe.buffer, n_new_neighbors);
			}
			DWORD qc_size = init_data->q[*init_data->qc].size();
			*init_data->working -= 1;
			if (*init_data->working == 0 && qc_size == 0) {
				*init_data->level += 1;
				DWORD qf_size = init_data->q[*init_data->qf].size();
				if (qf_size == 0) {
					// no more rooms to explore, ending search
					SetEvent(*init_data->end);
					*init_data->finished = true;
					break;
				}
				*init_data->qc = 1 - *init_data->qc;
				*init_data->qf = 1 - *init_data->qf;
				char* qf_size_with_commas;
				format_with_commas(qf_size_with_commas, qf_size);
				printf("--------- Switching to level %d with %s nodes\n", *init_data->level, qf_size_with_commas);
				ReleaseSemaphore(*init_data->sema, (qf_size+(MAX_BATCH-1))/MAX_BATCH, NULL);
			}
			ReleaseMutex(*init_data->mutex);		
		}

		// disconnecting from robot and closing out
		pipe.write(DISCONNECT, 0, 0, 0, 0, 0, robot_i);
		pipe.close_pipe();

		return 0;
	};

	static DWORD interpret_connection(void* buffer) {
		robot_connection_response* response = (robot_connection_response*)buffer;

		if (response->status != STATUS_OK) {
			printf("Robot connection failed with status %d for room %d\n", response->status, response->room_id);
			exit(-1);
		}

		return response->room_id;
	};
	static void interpret_move(DWORD& n_new_neighbors, void*& buffer, DWORD bytes_read, bitmap& d, 
		int robot_i, int level, DWORD*& rooms, LONG64 n_processed, LONG64& n_total_neighbors) {
		DWORD bytes_interpreted = 0;
		int cur_room = 0;

		response_robot_header* header = (response_robot_header*)((char*)buffer + bytes_interpreted);
		if (header->status != STATUS_OK) {
			printf("Robot move failed with status %d\n", header->status);
			exit(-1);
		}

		// adding new rooms to buffer
		int len = header->len;
		for (int i = 0; i < len; i++) {
			DWORD room_id = *((DWORD*)((char*)buffer + bytes_interpreted + sizeof(response_robot_header) + sizeof(DWORD) * i));
			if (d.interlocked_test_and_set(room_id)) {
				((DWORD*)buffer)[n_new_neighbors] = room_id;
				n_new_neighbors++;
			}
		}
		bytes_interpreted += sizeof(response_robot_header) + sizeof(DWORD) * len;
		n_total_neighbors += len;
		cur_room++;

		while (bytes_interpreted < bytes_read) {
			header = (response_robot_header*)((char*)buffer + bytes_interpreted);
			if (header->status != STATUS_OK) {
				printf("Robot move failed with status %d\n", header->status);
				exit(-1);
			}

			// adding new rooms to buffer
			len = header->len;
			for (int i = 0; i < len; i++) {
				DWORD room_id = *((DWORD*)((char*)buffer + bytes_interpreted + sizeof(response_robot_header) + sizeof(DWORD) * i));
				if (d.interlocked_test_and_set(room_id)) {
					((DWORD*)buffer)[n_new_neighbors] = room_id;
					n_new_neighbors++;
				}
			}
			if (len == 0) {
				char* n_processed_with_commas;
				format_with_commas(n_processed_with_commas, n_processed);
				printf("*** Thread [%d]: found exit room %lX, distance %d, rooms explored %s\n", robot_i, rooms[cur_room], level, n_processed_with_commas);
			}
			cur_room++;
			bytes_interpreted += sizeof(response_robot_header) + sizeof(DWORD) * len;
			n_total_neighbors += len;
		}
	};
};

