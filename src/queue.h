#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  void **buf;
  int cap;
  int head; // pop
  int tail; // push
  int count;
  pthread_mutex_t m;
  pthread_cond_t  can_push;
  pthread_cond_t  can_pop;
  bool shutdown;
} spscq_t;

bool spscq_init(spscq_t *q, int cap);
void spscq_destroy(spscq_t *q);
bool spscq_push(spscq_t *q, void *item);
bool spscq_pop(spscq_t *q, void **out);
void spscq_signal_shutdown(spscq_t *q);
