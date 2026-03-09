// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "Pipe.h"

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
			ReleaseMutex(*init_data->mutex);
			ReleaseSemaphore(*init_data->sema, 1, NULL);
		}

		// creating waiter handles
		HANDLE wait_handles[2] = { *init_data->end, *init_data->sema };

		// calculating halfway discovered num
		DWORD halfway_discovered = (1ULL << init_data->planet) >> 1;

		while (true) {
			// check if done or can proceed
			if (WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE) == WAIT_OBJECT_0) {
				break;
			}

			// popping batch of room to explore
			DWORD* rooms;
			WaitForSingleObject(*init_data->mutex, INFINITE);
			DWORD n_popped = init_data->q[*init_data->qc].pop(rooms, MAX_BATCH);
			*init_data->working += 1;
			ReleaseMutex(*init_data->mutex);

			// exploring rooms
			auto start_time = std::clock();
			pipe.write(MOVE, 0, 0, 0, rooms, n_popped, robot_i);
			pipe.read();
			auto end_time = std::clock();
			printf("Robot %d explored %d rooms in %f seconds\n", robot_i, n_popped, (double)(end_time - start_time) / CLOCKS_PER_SEC);
			DWORD* neighbors;
			DWORD n_neighbors;
			interpret_move(neighbors, n_neighbors, pipe.buffer, pipe.last_bytes_read);

			// checking if found exit
			if (n_neighbors == 0) {
				printf("Robot %d found target in room %d ----------------------------------------------------------------------------------\n", robot_i, rooms[0]);
				break;
			}

			// extracting rooms to add
			DWORD* new_neighbors;
			DWORD  n_new_neighbors = 0;
			if (init_data->d->read_nd() < halfway_discovered && false) {
				// effecient for more new discoveries
				DWORD n_removed = 0;
				for (int i = 0; i < n_neighbors; i++) {
					if (!(init_data->d->interlocked_test_and_set(neighbors[i-n_removed]))) {
						memmove(neighbors + i - n_removed, neighbors + i - n_removed + 1, sizeof(DWORD) * (n_neighbors - (i+1)));
						n_removed++;
					}
				}
				n_new_neighbors = n_neighbors - n_removed;
				new_neighbors = (DWORD*)malloc(sizeof(DWORD) * n_new_neighbors);
				memcpy(new_neighbors, neighbors, sizeof(DWORD) * n_new_neighbors);
				free(neighbors);
			}
			else {
				// effeceint for fewer new discoveries
				DWORD n_kept = 0;
				new_neighbors = (DWORD*)malloc(sizeof(DWORD) * n_neighbors);
				for (int i = 0; i < n_neighbors; i++) {
					if (init_data->d->interlocked_test_and_set(neighbors[i])) {
						new_neighbors[n_kept] = neighbors[i];
						n_kept++;
					}
				}
				n_new_neighbors = n_kept;
				free(neighbors);
			}

			// adding new rooms to qf
			if (n_new_neighbors > 0) {
				WaitForSingleObject(*init_data->mutex, INFINITE);
				init_data->q[*init_data->qf].push(new_neighbors, n_new_neighbors);
				ReleaseMutex(*init_data->mutex);
			}
			free(new_neighbors);

			// swapping qc and qf
			WaitForSingleObject(*init_data->mutex, INFINITE);
			DWORD qc_size = init_data->q[*init_data->qc].size();
			*init_data->working -= 1;
			printf("currently working: %d, current qc size: %d\n", *init_data->working, qc_size);
			if (*init_data->working == 0 && qc_size == 0) {
				DWORD qf_size = init_data->q[*init_data->qf].size();
				if (qf_size == 0) {
					// no more rooms to explore, ending search
					SetEvent(*init_data->end);
				}
				*init_data->qc = 1 - *init_data->qc;
				*init_data->qf = 1 - *init_data->qf;
				printf("Robot %d swapping queues. New qc: %d, new qf: %d, new qc size: %d\n", robot_i, *init_data->qc, *init_data->qf, qf_size);
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
	static void interpret_move(DWORD*& neighbors, DWORD& n_neighbors, void* buffer, DWORD bytes_read) {
		DWORD bytes_interpreted = 0;
		int cnt = 1;

		response_robot_header* header = (response_robot_header*)((char*)buffer + bytes_interpreted);
		if (header->status != STATUS_OK) {
			printf("Robot move failed with status %d\n", header->status);
			exit(-1);
		}

		neighbors = (DWORD*)malloc(sizeof(DWORD) * header->len);
		memcpy((void*)neighbors, (char*)buffer + bytes_interpreted + sizeof(response_robot_header), sizeof(DWORD) * header->len);
		n_neighbors = header->len;
		bytes_interpreted += sizeof(response_robot_header) + sizeof(DWORD) * header->len;

		while (bytes_interpreted < bytes_read) {
			cnt++;
			header = (response_robot_header*)((char*)buffer + bytes_interpreted);
			if (header->status != STATUS_OK) {
				printf("Robot move failed with status %d\n", header->status);
				exit(-1);
			}

			neighbors = (DWORD*)realloc(neighbors, sizeof(DWORD) * (n_neighbors + header->len));
			memcpy((void*)(neighbors+n_neighbors), (char*)buffer + bytes_interpreted + sizeof(response_robot_header), sizeof(DWORD) * header->len);
			n_neighbors += header->len;
			bytes_interpreted += sizeof(response_robot_header) + sizeof(DWORD) * header->len;
		}
	};
};