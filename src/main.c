#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "queue.h"

typedef struct { long id; struct timespec t_in; } item_t;

static volatile int running = 1;
static long produced=0, consumed=0, dropped=0;
static int prod_interval_us = 1000;
static int duration_s = 10;
static int queue_cap = 64;
static int pin_consumer_cpu = -1;
static int pin_producer_cpu = -1;
static const char* lat_out = NULL;
static const char* stats_out = NULL;
static size_t work_bytes = 0;   // Amount of work (in bytes) to simulate in the consumer per item

static inline long ns_diff(const struct timespec* a, const struct timespec* b){
  return (a->tv_sec - b->tv_sec)*1000000000L + (a->tv_nsec - b->tv_nsec);
}
static inline void now(struct timespec* t){ clock_gettime(CLOCK_MONOTONIC_RAW, t); }

void* producer(void* arg){
  spscq_t* q = (spscq_t*)arg;
  struct timespec start, nowts; now(&start);
  long id=0;

  if(pin_producer_cpu>=0){
      cpu_set_t set; CPU_ZERO(&set); CPU_SET(pin_producer_cpu, &set);
      if(sched_setaffinity(0, sizeof(set), &set) != 0) {
          perror("sched_setaffinity producer");
      }
    }

  // Setup for nanosleep
  struct timespec sleep_req;
  if (prod_interval_us > 0) {
      sleep_req.tv_sec = prod_interval_us / 1000000;
      sleep_req.tv_nsec = (prod_interval_us % 1000000) * 1000;
  }

  while(running){
    now(&nowts);
    if(ns_diff(&nowts,&start) >= (long)duration_s*1000000000L) break;
    
    item_t* it = malloc(sizeof(item_t));
    if (!it) {
        // Handle allocation failure, e.g., stop producer
        fprintf(stderr, "Producer malloc failed. Stopping.\n");
        break; 
    }
    it->id = id++; now(&it->t_in);

    // Non-blocking push: drop if full
    // !!! WARNING: This logic has a critical race condition. See review below. !!!
    pthread_mutex_lock(&q->m);
    int full = (q->count==q->cap);
    pthread_mutex_unlock(&q->m);
    
    if(full){
      dropped++; free(it);
    }else{
      spscq_push(q, it);
      produced++;
    }

    if(prod_interval_us > 0) nanosleep(&sleep_req, NULL);
  }
  spscq_signal_shutdown(q);
  return NULL;
}

void* consumer(void* arg){
  spscq_t* q = (spscq_t*)arg;
  if(pin_consumer_cpu>=0){
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(pin_consumer_cpu, &set);
    if(sched_setaffinity(0, sizeof(set), &set) != 0) {
          perror("sched_setaffinity producer");
      }
  }

  FILE* latf = NULL;
  if(lat_out) latf = fopen(lat_out, "w");

  enum { LAT_BATCH = 65536 };
  uint64_t *latbuf = latf ? (uint64_t*)malloc(sizeof(uint64_t)*LAT_BATCH) : NULL;
  size_t latn = 0;

  // Simulate fixed workload (optional)
  unsigned char* buf = NULL;
  if(work_bytes > 0) {
    buf = (unsigned char*)malloc(work_bytes);
    memset(buf, 0xA5, work_bytes);
  }

  void* pv;
  while(spscq_pop(q, &pv)){
    struct timespec t2; now(&t2);
    item_t* it = (item_t*)pv;

    // Simulate work: read/write a memory buffer
    if(work_bytes > 0) {
      // Simple touch to prevent compiler optimization
      for(size_t i=0;i<work_bytes;i+=64) buf[i] ^= (unsigned char)(it->id & 0xFF);
    }

    long ns = ns_diff(&t2, &it->t_in);
    // if(latf) fprintf(latf, "%ld\n", ns);
    if (latbuf) {
        latbuf[latn++] = ns;
        if (latn == LAT_BATCH) {
            for (size_t i = 0; i < LAT_BATCH; ++i) {
                fprintf(latf, "%llu\n", (unsigned long long)latbuf[i]);
            }
            latn = 0;
        }
    }

    consumed++;
    free(it);
  }
  // if (latbuf && latn) fwrite(latbuf, sizeof(uint64_t), latn, latf);

  for (size_t i = 0; i < latn; ++i)
    fprintf(latf, "%llu\n", (unsigned long long)latbuf[i]);
  if (latbuf) free(latbuf);
  if(buf) free(buf);
  if(latf) fclose(latf);
  return NULL;
}

static void usage(const char* prog){
  fprintf(stderr,
    "Usage: %s [--duration S] [--prod-interval-us US] [--queue-cap N]\n"
    "         [--pin-consumer CPU] [--pin-producer CPU] [--emit-latencies FILE]\n"
    "         [--stats FILE] [--work-bytes N]\n",
    prog);
}

int main(int argc, char** argv){
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--duration") && i+1<argc){ duration_s = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--prod-interval-us") && i+1<argc){ prod_interval_us = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--queue-cap") && i+1<argc){ queue_cap = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--pin-consumer") && i+1<argc){ pin_consumer_cpu = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--pin-producer") && i+1<argc){ pin_producer_cpu = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--emit-latencies") && i+1<argc){ lat_out = argv[++i]; }
    else if(!strcmp(argv[i],"--stats") && i+1<argc){ stats_out = argv[++i]; }
    else if(!strcmp(argv[i],"--work-bytes") && i+1<argc){ work_bytes = strtoull(argv[++i], NULL, 10); }
    else { usage(argv[0]); return 1; }
  }

  struct timespec t_start, t_end; now(&t_start);

  spscq_t q; if(!spscq_init(&q, queue_cap)) { perror("queue"); return 1; }
  pthread_t pt, ct;
  pthread_create(&pt, NULL, producer, &q);
  pthread_create(&ct, NULL, consumer, &q);
  pthread_join(pt, NULL);
  pthread_join(ct, NULL);
  spscq_destroy(&q);

  now(&t_end);
  double duration_real_s = ns_diff(&t_end, &t_start) / 1e9;

  // Standard error output (for human reading)
  fprintf(stderr, "produced=%ld consumed=%ld dropped=%ld duration_s=%.6f\n",
          produced, consumed, dropped, duration_real_s);

  // Stats file (for script parsing)
  if(stats_out){
    FILE* sf = fopen(stats_out, "w");
    if(sf){
      fprintf(sf, "produced,%ld\nconsumed,%ld\ndropped,%ld\nduration_s,%.6f\n",
              produced, consumed, dropped, duration_real_s);
      fclose(sf);
    }
  }
  return 0;
}