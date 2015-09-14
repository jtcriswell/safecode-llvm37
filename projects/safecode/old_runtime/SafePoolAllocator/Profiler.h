/// A simple profiler using performance counter

#ifndef _SC_PROFILER_H_
#define _SC_PROFILER_H_

#include "safecode/SAFECode.h"

#include <stdint.h>
#include <stdlib.h>
#include "rdtsc.h"
#define ENABLE_PROFILING 1
#define SAMPLING_FACTOR	32
#define ENABLE_SAMPLING 1
#undef ENABLE_PROFILING

// HACK
#ifdef __linux__
#define LOG_FN_TMPL "/localhome/mai4/profiler.%s.dat"
#else
#define LOG_FN_TMPL "/Users/mai4/work/data/profiler.%s.dat"
#endif

#ifdef ENABLE_PROFILING
#define PROFILING(X) X
#else
#define PROFILING(X)
#endif

NAMESPACE_SC_BEGIN

// Log info for time sync
void profile_sync_point(unsigned long long start_time, unsigned long long end_time, unsigned int queue_size);

// Log info for enque
void profile_enqueue(unsigned long long start_time, unsigned long long end_time);

// Print a log to profiler
void profile_queue_op(int type, unsigned long long start_time, unsigned long long end_time);

NAMESPACE_SC_END

#endif
