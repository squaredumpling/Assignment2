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

/* 
 * curr_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Arrival curr_arrivals[4][4][20];

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
  // TODO:
  // while not all arrivals have been handled, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)

  return(0);
}

static pthread_mutex_t      basic_intersection          = PTHREAD_MUTEX_INITIALIZER;


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
  pthread_t threads[10];
  for (int i = 0; i < 10; i++) {
    int* thread_id = malloc(sizeof(int));  // we have to think what value we need to pass as arg for manage_light
    *thread_id = i;

    int rtnval = pthread_create(&threads[i], NULL, manage_light, thread_id);
    if (rtnval != 0) {
      printf("Thread creation failed with error code: %d\n", rtnval);
      exit(1);
    }
    printf("Thread %d created successfully\n", i);
  }

  // TODO: create a thread that executes supply_arrivals
  pthread_t supply_thread;
  int rtnval = pthread_create(&supply_thread, NULL, supply_arrivals, NULL);
  if (rtnval != 0) {
    printf("Supply thread creation failed with error code: %d\n", rtnval);
    exit(1);
  }
  printf("Supply thread %d created successfully\n");

  // TODO: wait for all threads to finish
  for (int i = 0; i < 10; i++) {
    rtnval = pthread_join(threads[i], NULL);
    if (rtnval != 0) {
      printf("Thread %d join failed with error code: %d\n", i, rtnval);
      exit(2);
    }
    printf("Thread %d joined\n", i);
  }

  rtnval = pthread_join(supply_thread, NULL);
  if (rtnval != 0) {
    printf("Supply thread join failed with error code: %d\n", rtnval);
    exit(2);
  }
  printf("Supply thread %d joined\n");

  // destroy semaphores
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_destroy(&semaphores[i][j]);
    }
  }

  return 0;
}
