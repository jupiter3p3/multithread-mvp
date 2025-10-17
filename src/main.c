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
static int prod_interval_us = 1000; // per work 1ms
static int duration_s = 10;
static int queue_cap = 64;
static int pin_consumer_cpu = -1;
static const char* lat_out = NULL;

static inline long ns_diff(const struct timespec* a, const struct timespec* b){
  return (a->tv_sec - b->tv_sec)*1000000000L + (a->tv_nsec - b->tv_nsec);
}
static inline void now(struct timespec* t){ clock_gettime(CLOCK_MONOTONIC_RAW, t); }

void* producer(void* arg){
  spscq_t* q = (spscq_t*)arg;
  struct timespec start, nowts; now(&start);
  long id=0;
  while(running){
    now(&nowts);
    if(ns_diff(&nowts,&start) >= (long)duration_s*1000000000L) break;
    item_t* it = malloc(sizeof(item_t));
    it->id = id++; now(&it->t_in);

    // if full drop (cal drop）
    pthread_mutex_lock(&q->m);
    int full = (q->count==q->cap);
    pthread_mutex_unlock(&q->m);
    if(full){
      dropped++; free(it);
    }else{
      spscq_push(q, it);
      produced++;
    }
    usleep(prod_interval_us);
  }
  spscq_signal_shutdown(q);
  return NULL;
}

void* consumer(void* arg){
  spscq_t* q = (spscq_t*)arg;
  if(pin_consumer_cpu>=0){
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(pin_consumer_cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
  }
  FILE* latf = NULL;
  if(lat_out) latf = fopen(lat_out, "w");
  void* pv;
  while(spscq_pop(q, &pv)){
    struct timespec t2; now(&t2);
    item_t* it = (item_t*)pv;
    long ns = ns_diff(&t2, &it->t_in);
    if(latf) fprintf(latf, "%ld\n", ns);
    consumed++;
    free(it);
  }
  if(latf) fclose(latf);
  return NULL;
}

static void usage(const char* prog){
  fprintf(stderr,
    "Usage: %s [--duration 10s] [--prod-interval-us 1000] [--queue-cap 64] [--pin-consumer <cpu>] [--emit-latencies file]\n",
    prog);
}

int main(int argc, char** argv){
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--duration") && i+1<argc){ duration_s = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--prod-interval-us") && i+1<argc){ prod_interval_us = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--queue-cap") && i+1<argc){ queue_cap = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--pin-consumer") && i+1<argc){ pin_consumer_cpu = atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--emit-latencies") && i+1<argc){ lat_out = argv[++i]; }
    else { usage(argv[0]); return 1; }
  }

  spscq_t q; if(!spscq_init(&q, queue_cap)) { perror("queue"); return 1; }

  pthread_t pt, ct;
  pthread_create(&pt, NULL, producer, &q);
  pthread_create(&ct, NULL, consumer, &q);

  pthread_join(pt, NULL);
  pthread_join(ct, NULL);
  spscq_destroy(&q);

  fprintf(stderr, "produced=%ld consumed=%ld dropped=%ld duration_s=%d\n", produced, consumed, dropped, duration_s);
  return 0;
}
