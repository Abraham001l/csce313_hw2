// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "SearchStructures.h"
#define uchar UCHAR
#define ushort unsigned short
#define uint64 unsigned __int64
#define CONNECT 0
#define DISCONNECT 1
#define MOVE 2
#define FAILURE 0
#define SUCCESS 1
#define MAX_BATCH 10000
#define STATUS_OK 0 // no error, command successful
#define STATUS_ALREADY_CONNECTED 1 // repeated attempt to connect
#define STATUS_INVALID_COMMAND 2 // command too short or invalid
#define STATUS_INVALID_ROOM 3 // room ID doesn't exist
#define STATUS_INVALID_BATCH_SIZE 4 // batch size too large or equals 0
#define STATUS_MUST_CONNECT 5 // first command must be CONNECT

class Pipe {
public:
	bool is_cc;
	HANDLE pipe;
	DWORD buffer_size;
	DWORD last_bytes_read;
	DWORD last_bytes_written;
	void* buffer;
	char* robot_write;


	Pipe();
	~Pipe();
	void create_pipe(PROCESS_INFORMATION& pi, bool is_cc, int robot_i);
	void close_pipe();
	void write(ushort command, uchar planet, DWORD cave, ushort robots, DWORD* room_ids, int n_rooms, int robot_i);
	void read();
};

#pragma pack(push, 1)
class command_cc {
public:
	uchar command : 2; // lower 2 bits
	uchar planet : 6; // remaining 6 bits
	DWORD cave;
	ushort robots;
};
#pragma pack(pop)

#pragma pack(push, 1)
class response_cc {
public:
	DWORD status;
	char msg[64];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct robot_init {
	PROCESS_INFORMATION pi;
	int robot_i;
	HANDLE* mutex;
	HANDLE* sema;
	HANDLE* end;
	queue* q;
	bitmap* d;
	UCHAR planet;
	DWORD* working;
	bool* qc;
	bool* qf;
	int* level;
	LONG64* n_rooms_processed;
	LONG64* n_total_returns;
	LONG64* n_unique_returns;
	bool* finished;
};
#pragma pack(pop)

#pragma pack(push, 1)
class command_robot_header {
public:
	DWORD command;
};
#pragma pack(pop)

#pragma pack(push, 1)
class response_robot_header {
public:
	DWORD status : 3;
	DWORD len : 29;
};
#pragma pack(pop)

#pragma pack(push, 1)
class robot_connection_response {
public:
	DWORD status : 3;
	DWORD room_id;
};
#pragma pack(pop)