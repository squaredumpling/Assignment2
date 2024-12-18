#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"
#include "intersection_time.h"
#include "input.h"

typedef struct {
  int side, direction, thread_id;
  bool is_valid;
  sem_t* semaphore;
} TrafficLight;

static pthread_mutex_t      basic_intersection          = PTHREAD_MUTEX_INITIALIZER;

/* 
 * curr_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Arrival curr_arrivals[4][4][20];

int getNextArrival(const int side, const int direction, Arrival** arrival) {
  static int indexes[4][4] = {0};
  static int N_ARRIVALS = sizeof(input_arrivals) / sizeof(Arrival);
  static int total_cars_handled = 0;

  // printf("(%d, %d) %d - %d - %p\n", side, direction, indexes[side][direction], total_cars_handled, (void*)*arrival);
  if (total_cars_handled == N_ARRIVALS) return -1;  // all cars have been handled

  int i = indexes[side][direction];
  // int old_side = arrival->side, old_direction = arrival->direction, old_id = arrival->id, old_time = arrival->time;
  Arrival* old_arrival_ptr = *arrival;
  *arrival = &curr_arrivals[side][direction][i];
  Arrival current_arrival = **arrival;
  if (current_arrival.side == NORTH && old_arrival_ptr) return 0;  // Waiting for supply_arrivals to update value of curr_arrivals[side][direction][i]
  // printf("New arrival address %p\n", (void*)*arrival);
  total_cars_handled += indexes[side][direction]++;
  // printf(" %d - %d\n", indexes[side][direction], total_cars_handled);

  return 0;
}

/*
 * semaphores[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t semaphores[4][4];

/*
 * supply_arrivals()
 *
 * A function for supplying arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_arrivals()
{
  int t = 0;
  int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

  // for every arrival in the list
  for (int i = 0; i < sizeof(input_arrivals)/sizeof(Arrival); i++)
  {
    // get the next arrival in the list
    Arrival arrival = input_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep(arrival.time - t);
    t = arrival.time;
    // store the new arrival in curr_arrivals
    curr_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;
    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&semaphores[arrival.side][arrival.direction]);
  }

  return(0);
}


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
  TrafficLight tf = *(TrafficLight*)arg;
  free(arg);

  // printf("On thread %lx (%d)\n", pthread_self(), tf.thread_id);
  // printf("Side: %d\nDirection: %d\nIs valid: %d\n", tf.side, tf.direction, (int)tf.is_valid);

  struct timespec timeout;
  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += END_TIME; // how many seconds need to pass to trigger timeout

  Arrival* arrival = NULL;
  while (tf.is_valid == true && getNextArrival(tf.side, tf.direction, &arrival) == 0) {

    // printf("Side: %d Direction: %d ID: %d Time %d\n", arrival->side, arrival->direction, arrival->id, arrival->time);
    // printf("Time left 'till timeout: %ds\n", END_TIME - get_time_passed());
    if (sem_timedwait(tf.semaphore, &timeout) == -1)  break;
    // printf("(%d) New Side: %d Direction: %d ID: %d Time %d\n", tf.thread_id, arrival->side, arrival->direction, arrival->id, arrival->time);

    pthread_mutex_lock(&basic_intersection);
    printf("traffic light %d %d turns green at %d for car %d\n", tf.side, tf.direction, get_time_passed(), arrival->id);

    sleep(CROSS_TIME);

    printf("traffic light %d %d turns red at %d\n", tf.side, tf.direction, get_time_passed());
    pthread_mutex_unlock(&basic_intersection);
  }

  // free(arrival);
  // TODO:
  // while not all arrivals have been ha- get_time_passed()ndled, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)
  // printf("Exit thread %lx (%d)\n", pthread_self(), tf.thread_id);
  return(0);
}


int main(int argc, char * argv[])
{
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_init(&semaphores[i][j], 0, 0);
    }
  }

  // start the timer
  start_time();


  // TODO: create a thread per traffic light that executes manage_light
  pthread_t threads[16];
  for (int i = 0; i < 16; i++) {
    TrafficLight* tf = malloc(sizeof(TrafficLight));
    tf->side = i / 4;
    tf->direction = i % 4;
    tf->is_valid = true;
    tf->thread_id = i;
    tf->semaphore = &semaphores[tf->side][tf->direction];

    if (tf->side == 0 || (tf->side % 2 == 1 && tf->direction == 3)) tf->is_valid = false;

    int errNo = pthread_create(&threads[i], NULL, manage_light, tf);
    if (errNo != 0) {
      printf("Thread creation failed with error code: %d\n", errNo);
      exit(1);
    }
    // printf("Thread %d created successfully\n", i);
  }


  // TODO: create a thread that executes supply_arrivals
  pthread_t supply_thread;
  int errNo = pthread_create(&supply_thread, NULL, supply_arrivals, NULL);
  if (errNo != 0) {
    printf("Supply thread creation failed with error code: %d\n", errNo);
    exit(1);
  }
  // printf("Supply thread created successfully\n");

  // TODO: wait for all threads to finish
  errNo = pthread_join(supply_thread, NULL);
  if (errNo != 0) {
    printf("Supply thread join failed with error code: %d\n", errNo);
    exit(2);
  }
  // printf("Supply thread joined\n");

  for (int i = 0; i < 16; i++) {
    errNo = pthread_join(threads[i], NULL);
    if (errNo != 0) {
      printf("Thread %d join failed with error code: %d\n", i, errNo);
      exit(2);
    }
    // printf("Thread %d joined\n", i);
  }


  // destroy semaphores
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_destroy(&semaphores[i][j]);
    }
  }

  // printf("Elapsed time: %d\n", get_time_passed());
  // printf("All threads finished\n");
  return 0;
}
