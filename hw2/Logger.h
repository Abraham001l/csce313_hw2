// Abraham Lira, CSCE 313 Section 200, Spring 2026

#pragma once

#include "pch.h"
#include "Pipe.h"

#pragma pack(push, 1)
class logger_data {
public:
	HANDLE* mutex;
	HANDLE* end;
	queue* q;
	bitmap* d;
	bool* qc;
	bool* qf;
	DWORD* working;
	CPU* cpu_stats;
	LONG64* n_rooms_processed;
	LONG64* n_total_returns;
	LONG64* n_unique_returns;
	bool* finished;
};
#pragma pack(pop)

class Logger {
public:
	static DWORD WINAPI log(LPVOID lg_data) {
		logger_data* data = (logger_data*)lg_data;

		LONG64 last_n_processed = 0;
		LONG64 last_n_discovered = 0;
		LONG64 last_n_returns = 0;

		while (true) {
			auto start_time = std::clock();
			Sleep(2000);
			double time_elapsed = double(std::clock() - start_time) / CLOCKS_PER_SEC;

			// printing
			if (*data->finished) {
				break;
			}
			LONG64 cur_qc_size = data->q[*data->qc].size(); // current size limit
			LONG64 cur_n_discovered = *data->n_unique_returns; // D metric
			LONG64 cur_n_processed = *data->n_rooms_processed; // U metric
			LONG64 cur_n_returns = *data->n_total_returns;

			LONG64 delta_n_processed = cur_n_processed - last_n_processed; // used for RPS
			LONG64 delta_n_discovered = cur_n_discovered - last_n_discovered; // used for uniq
			LONG64 delta_n_returns = cur_n_returns - last_n_returns; // used for uniq

			// Percent unique compares newly discovered nodes to TOTAL incoming replies/edges 
			int uniq_percent = 0;
			if (delta_n_returns > 0) {
				uniq_percent = (int)((delta_n_discovered * 100ULL) / delta_n_returns);
				// Clamp to 100 max to account for minor thread syncing offsets
				if (uniq_percent > 100) {
					uniq_percent = 100;
				}
			}

			// Double check active Threads specifically
			DWORD active_working = *(data->working);

			printf("[%.1fM] U %.1fM D %.1fM, %.2fM/sec, %d*, %d%% uniq [%d%% CPU, %d MB]\n",
				cur_n_processed / 1000000.0,
				cur_qc_size / 1000000.0,
				cur_n_discovered / 1000000.0,
				delta_n_processed / (1000000.0 * time_elapsed),
				active_working,
				uniq_percent,
				(int)data->cpu_stats->GetCpuUtilization(NULL),
				data->cpu_stats->GetProcessRAMUsage(true));

			last_n_discovered = cur_n_discovered;
			last_n_processed = cur_n_processed;
			last_n_returns = cur_n_returns;
		}
		return 0;
	}
};