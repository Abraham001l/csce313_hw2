// Abraham Lira, CSCE 313 Section 200, Spring 2026

#include "pch.h"
#include "Pipe.h"

Pipe::Pipe() : pipe(nullptr), buffer_size(40000), last_bytes_read(0), last_bytes_written(0), buffer(malloc(40000)) {}

Pipe::~Pipe() {
	free(buffer);
	if (robot_write) {
		free(robot_write);
	}
}

void Pipe::close_pipe() {
	CloseHandle(pipe);
}

void Pipe::create_pipe(PROCESS_INFORMATION& pi, bool is_cc, int robot_i) {
	this->is_cc = is_cc;
	char pipe_name[64];

	if (is_cc) { // cc
		sprintf_s(pipe_name, sizeof(pipe_name), "\\\\.\\pipe\\CC-%X", pi.dwProcessId);
		while (WaitNamedPipe(pipe_name, INFINITE) == FALSE)
			Sleep(100);

		pipe = CreateFile(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (pipe == INVALID_HANDLE_VALUE)
		{
			printf("Error %d creating pipe to %s\n", GetLastError(), pipe_name);
			exit(-1);
		}
	}
	else { // robot
		sprintf_s(pipe_name, sizeof(pipe_name), "\\\\.\\pipe\\CC-%X-robot-%X", pi.dwProcessId, robot_i);
		while (WaitNamedPipe(pipe_name, INFINITE) == FALSE) {
			Sleep(100);
		}

		pipe = CreateFile(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (pipe == INVALID_HANDLE_VALUE)
		{
			printf("Error %d creating pipe to %s\n", GetLastError(), pipe_name);
			exit(-1);
		}

		// allocating buffer for robot write
		robot_write = (char*)malloc(sizeof(command_robot_header)+sizeof(DWORD)*MAX_BATCH);
	}

}

void Pipe::write(ushort command, uchar planet, DWORD cave, ushort robots, DWORD* room_ids, int n_rooms, int robot_i) {
	if (is_cc) { // cc
		command_cc cc_cmd = { command, planet, cave, robots };
		bool write_status = WriteFile(pipe, &cc_cmd, sizeof(cc_cmd), &last_bytes_written, NULL);

		if (write_status == FALSE)
		{
			printf("Error %d writing to CC\n", GetLastError());
			exit(-1);
		}
	}
	else { // robot
		command_robot_header header = { command };
		memcpy(robot_write, &header, sizeof(header));
		memcpy((char*)robot_write + sizeof(header), room_ids, sizeof(DWORD) * n_rooms);
		bool write_status = WriteFile(pipe, robot_write, sizeof(header)+ sizeof(DWORD) * n_rooms, &last_bytes_written, NULL);

		if (write_status == FALSE)
		{
			printf("Error %d writing to robot pipe %d\n", GetLastError(), robot_i);
			exit(-1);
		}
	}
	
}

void Pipe::read() {
	if (is_cc) { // cc
		response_cc cc_response;
		bool read_status = ReadFile(pipe, &cc_response, sizeof(cc_response), &last_bytes_read, NULL);
		
		if (read_status == FALSE)
		{
			printf("Error %d reading from CC\n", GetLastError());
			exit(-1);
		}

		if (cc_response.status == FAILURE) {
			printf("CC responded with failure: %s\n", cc_response.msg);
			exit(-1);
		}
	}
	else { // robot
		bool read_status = ReadFile(pipe, buffer, buffer_size, &last_bytes_read, NULL);

		if (last_bytes_read == buffer_size) {
			// checking how many bytes leftover
			DWORD bytes_leftover = 0;
			bool peek_status = PeekNamedPipe(pipe, NULL, 0, NULL, &bytes_leftover, NULL);

			if (peek_status == FALSE)
			{
				printf("Error %d peeking robot pipe\n", GetLastError());
				exit(-1);
			}

			if (bytes_leftover > 0) {
				// creating resized buffer
				DWORD new_buffer_size = buffer_size + bytes_leftover;

				void* temp = realloc(buffer, new_buffer_size);
				if (temp == NULL) {
					printf("Error %d reallocating buffer for robot pipe\n", GetLastError());
					exit(-1);
				}
				buffer = temp;

				// reading leftover bytes
				DWORD additional_bytes_read = 0;
				bool additional_read_status = ReadFile(pipe, (char*)buffer + buffer_size, bytes_leftover, &additional_bytes_read, NULL);

				if (additional_read_status == FALSE)
				{
					printf("Error %d reading leftover bytes from robot pipe\n", GetLastError());
					exit(-1);
				}

				last_bytes_read += additional_bytes_read;
			}
		}
	}
}