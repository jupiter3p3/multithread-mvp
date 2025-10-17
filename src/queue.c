#include "queue.h"
#include <stdlib.h>

bool spscq_init(spscq_t *q, int cap){
  q->buf = (void**)calloc(cap, sizeof(void*));
  if(!q->buf) return false;
  q->cap=cap; q->head=q->tail=q->count=0; q->shutdown=false;
  pthread_mutex_init(&q->m, NULL);
  pthread_cond_init(&q->can_push, NULL);
  pthread_cond_init(&q->can_pop, NULL);
  return true;
}
void spscq_destroy(spscq_t *q){
  pthread_cond_destroy(&q->can_push);
  pthread_cond_destroy(&q->can_pop);
  pthread_mutex_destroy(&q->m);
  free(q->buf);
}
bool spscq_push(spscq_t *q, void *item){
  pthread_mutex_lock(&q->m);
  while(q->count==q->cap && !q->shutdown){
    pthread_cond_wait(&q->can_push, &q->m);
  }
  if(q->shutdown){ pthread_mutex_unlock(&q->m); return false; }
  q->buf[q->tail] = item;
  q->tail = (q->tail+1)%q->cap;
  q->count++;
  pthread_cond_signal(&q->can_pop);
  pthread_mutex_unlock(&q->m);
  return true;
}
bool spscq_pop(spscq_t *q, void **out){
  pthread_mutex_lock(&q->m);
  while(q->count==0 && !q->shutdown){
    pthread_cond_wait(&q->can_pop, &q->m);
  }
  if(q->count==0 && q->shutdown){ pthread_mutex_unlock(&q->m); return false; }
  *out = q->buf[q->head];
  q->head = (q->head+1)%q->cap;
  q->count--;
  pthread_cond_signal(&q->can_push);
  pthread_mutex_unlock(&q->m);
  return true;
}
void spscq_signal_shutdown(spscq_t *q){
  pthread_mutex_lock(&q->m);
  q->shutdown = true;
  pthread_cond_broadcast(&q->can_pop);
  pthread_cond_broadcast(&q->can_push);
  pthread_mutex_unlock(&q->m);
}
