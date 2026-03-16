// Abraham Lira, CSCE 313 Section 200, Spring 2026


#include "pch.h"
#include "Pipe.h"
#include "Robot.h"
#include "SearchStructures.h"
#include "Logger.h"

void parse_input_arguments(int argc, char* argv[], UCHAR& planet, DWORD& cave, DWORD& num_threads) {
	if (argc != 4)
	{
		printf("Usage: caveSearch.exe <planet (1-7)> <cave number> <number of threads>\n");
		exit(-1);
	}
	planet = (UCHAR)atoi(argv[1]);
	cave = (DWORD)atoi(argv[2]);
	num_threads = (DWORD)atoi(argv[3]);
}

int main(int argc, char* argv[])
{
	// execution time setup
	auto start_time = std::clock();

	// parsing input arguments
	UCHAR planet;
	DWORD cave, num_threads;
	parse_input_arguments(argc, argv, planet, cave, num_threads);

	// creating cc
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	GetStartupInfo(&si);
	char path[] = "CC-hw2-2.4.exe";

	if (!CreateProcess(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		printf("Error %d creating CC process\n", GetLastError());
		exit(-1);
	}

	// creating pipe to cc
	Pipe cc_pipe;
	cc_pipe.create_pipe(pi, true, 0);

	// connecting to cc
	cc_pipe.write(CONNECT, planet, cave, num_threads, 0, 0, 0);
	cc_pipe.read();

	// creating d
	bitmap d;
	d.init();

	// creating qc and qf and q_holder
	queue q1;
	queue q2;
	queue q_holder[2] = { q1, q2 };

	// creating working counter for light switch problem
	DWORD working = 0;

	// initializing qc and qf
	bool qc = 0;
	bool qf = 1;

	// initializing level
	int level = 0;

	// initializing cpu stats
	CPU cpu_stats = CPU();

	// initializing vars for stats
	LONG64 n_rooms_processed = 0;
	LONG64 n_total_returns = 0;
	LONG64 n_unique_returns = 0;

	// initializing finished
	bool finished = false;

	// initializing mutex, semaphore, and end event for search
	HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
	HANDLE sema = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
	HANDLE end = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!mutex || !sema || !end) {
		printf("Error %d creating synchronization objects\n", GetLastError());
		exit(-1);
	}
	// running robots
	HANDLE* thread_handles = (HANDLE*)malloc(sizeof(HANDLE) * num_threads);
	robot_init* thread_params = (robot_init*)malloc(sizeof(robot_init) * num_threads);

	for (int i = 0; i < num_threads; i++) {
		thread_params[i] = { pi, i, &mutex, &sema, &end, q_holder, &d, planet, &working, &qc, &qf, &level, &n_rooms_processed, &n_total_returns, &n_unique_returns, &finished };
		thread_handles[i] = CreateThread(NULL, 0, Robot::search, &thread_params[i], 0, NULL);
		if (thread_handles[i] == NULL)
		{
			printf("Error %d creating thread for robot %d\n", GetLastError(), i);
			exit(-1);
		}
		SetThreadPriority(thread_handles[i], THREAD_PRIORITY_IDLE);
	}

	logger_data logger_data = { &mutex, &end, q_holder, &d, &qc, &qf, &working, &cpu_stats, &n_rooms_processed, &n_total_returns, &n_unique_returns, &finished };
	HANDLE logger_handle = CreateThread(NULL, 0, Logger::log, &logger_data, 0, NULL);

	for (int i = 0; i < num_threads; i++) {
		WaitForSingleObject(thread_handles[i], INFINITE);
		CloseHandle(thread_handles[i]);
	}
	WaitForSingleObject(logger_handle, INFINITE);
	CloseHandle(logger_handle);

	free(thread_handles);
	free(thread_params);

	// disconnect from cc and close out
	cc_pipe.write(DISCONNECT, planet, cave, num_threads, 0, 0, 0);
	cc_pipe.close_pipe();
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	// execution time output
	auto end_time = std::clock();
	double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;
	printf("Execution time: %.2f seconds\n", elapsed_time);

	return 0;
}