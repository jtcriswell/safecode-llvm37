//===- Profiler.cpp: ------------------------------------------------------===//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implementation of simple profiler
//
//===----------------------------------------------------------------------===//

#include "Profiler.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <map>

#ifdef HAVE_FOPEN64
#define FOPEN64 fopen64
#else
#define FOPEN64 fopen
#endif
 
NAMESPACE_SC_BEGIN


/*
struct profile_entry {
  unsigned char type;
  unsigned char tag;
  unsigned int duration;
  unsigned long long start_time;
} __attribute__ ((packed));
*/

struct profile_entry_sync_point {
  unsigned long long start_time;
  unsigned int duration;
  unsigned int queue_size;
};

struct profile_entry_enqueue {
  unsigned long long start_time;
  unsigned int duration;
};

struct profile_entry_queue_op {
  unsigned int type;
  unsigned long long start_time;
  unsigned int duration;
};


#if ENABLE_PROFILING
#define SEED(X) static unsigned int X = (unsigned int) (pthread_self())
#else
#define SEED(X)
#endif

#if ENABLE_PROFILING && ENABLE_SAMPLING
#define SAMPLING(RAND_SEED, INTERVAL, CODE) \
  do { \
    if (((double)rand_r(&RAND_SEED) * INTERVAL / RAND_MAX) < 1) { \
      CODE \
    }} \
  while (0);
#else
#define SAMPLING(RAND_SEED, INTERVAL, CODE) \
  do { \
      CODE \
  } while (0);
#endif

class Profiler {
private:
/*  static const size_t QUEUE_OP_COUNT = 8;
  static const size_t BUCKET_HISTOGRAM = 1 << 22;
  unsigned m_queue_op_count[QUEUE_OP_COUNT][BUCKET_HISTOGRAM];
*/
  FILE * h_sync_point;
  FILE * h_enqueue;
  FILE * h_queue_op;
public:
  Profiler() {
    char buf[512];
    snprintf(buf, sizeof(buf), LOG_FN_TMPL, "sync");
    h_sync_point = FOPEN64(buf, "wb");
    snprintf(buf, sizeof(buf), LOG_FN_TMPL, "enqueue");
    h_enqueue = FOPEN64(buf, "wb");
    snprintf(buf, sizeof(buf), LOG_FN_TMPL, "queue_op");
    h_queue_op = FOPEN64(buf, "wb");
//    memset(m_queue_op_count, 0, sizeof(m_queue_op_count));
  }

  ~Profiler() {
    fclose(h_sync_point);
    fclose(h_enqueue);
/*    for (size_t i = 0; i < QUEUE_OP_COUNT; ++i) {
      struct profile_entry_queue_op e;
      e.type = i;
      for(size_t j = 0; j < BUCKET_HISTOGRAM; ++j) {
        e.duration = j;
        e.count = m_queue_op_count[i][j];
        fwrite(&e, sizeof(struct profile_entry_queue_op), 1, h_queue_op);
      }
    } */
    fclose(h_queue_op);
  }

  void log(int type, unsigned long long start_time, unsigned long long end_time, unsigned int tag) {
/*    pthread_t tid = pthread_self();
    if (m_log_map.find(tid) == m_log_map.end()) {
      char buf[1024];
      snprintf(buf, sizeof(buf), DEFAULT_LOG_FILENAME, pthread_self());
      m_log_map[tid] = FOPEN64(buf, "wb");
      m_buf_map[tid] = new char[BUF_SIZE];
      setvbuf(m_log_map[tid], m_buf_map[tid], _IOFBF, BUF_SIZE);
    }
    FILE * m_log = m_log_map[tid];
    struct profile_entry e;
    e.type = type;
    e.tag = tag;
    e.start_time = start_time;
    e.duration = end_time - start_time;
    fwrite(&e, sizeof(struct profile_entry), 1, m_log); */
  }
  void profile_sync_point(unsigned long long start_time, unsigned long long end_time, unsigned int queue_size)
  {
    struct profile_entry_sync_point e;
    e.start_time = start_time;
    e.duration = end_time - start_time;
    e.queue_size = queue_size;
    fwrite(&e, sizeof(struct profile_entry_sync_point), 1, h_sync_point);
  }

  void profile_enqueue(unsigned long long start_time, unsigned long long end_time) {
    SEED(seed);
    SAMPLING(seed, SAMPLING_FACTOR, 
    struct profile_entry_enqueue e;
    e.start_time = start_time;
    e.duration = end_time - start_time;
    fwrite(&e, sizeof(struct profile_entry_enqueue), 1, h_enqueue);
    )
  }
  
  void profile_queue_op(int type, unsigned long long start_time, unsigned long long end_time) {
    SEED(seed);
    SAMPLING(seed, SAMPLING_FACTOR, 
    struct profile_entry_queue_op e;
    e.type = type; 
    e.start_time = start_time;
    e.duration = end_time - start_time;
    fwrite(&e, sizeof(struct profile_entry_queue_op), 1, h_queue_op);
    )
/*    unsigned int bucket = (end_time - start_time) >> 10;
    ++m_queue_op_count[type][bucket];
*/  
  }

    
};

PROFILING (static Profiler p;

void profiler_log(int type, unsigned long long start_time, unsigned long long end_time, unsigned int tag)
{
  p.log(type, start_time, end_time, tag);
}

// Log info for time sync
void profile_sync_point(unsigned long long start_time, unsigned long long end_time, unsigned int queue_size)
{
  p.profile_sync_point(start_time, end_time, queue_size);
}


// Log info for enque
void profile_enqueue(unsigned long long start_time, unsigned long long end_time) {
  p.profile_enqueue(start_time, end_time);
}

// Print a log to profiler
void profile_queue_op(int type, unsigned long long start_time, unsigned long long end_time) {
  p.profile_queue_op(type, start_time, end_time);
}
)
NAMESPACE_SC_END
