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
  printf("starting to supply\n");

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

  printf("done supplying\n");

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

  while(true) {
    break;
  }

  return(0);
}


int main(int argc, char * argv[])
{
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++){
    for (int j = 0; j < 4; j++){
      sem_init(&semaphores[i][j], 0, 0);
    }
  }

  // start the timer
  start_time();


  // create a thread per traffic light that executes manage_light
  pthread_t traffic_threads[10];
  for (int i = 0; i < 10; i++) {
    int traffic_create_error = pthread_create(&traffic_threads[i], NULL, manage_light, &i);
    if (traffic_create_error) { printf("Thread creation failed with error code: %d\n", traffic_create_error); exit(1); }
    printf("Thread %d created successfully\n", i);
  }

  // create a thread that executes supply_arrivals
  pthread_t supply_thread;
  int supply_create_error = pthread_create(&supply_thread, NULL, supply_arrivals, NULL);
  if (supply_create_error) { printf("Supply thread creation failed with code: %d\n", supply_create_error); exit(1); }
  printf("Supply thread created successfully\n");

  // wait for all traffic light threads to finish
  for (int i = 0; i < 10; i++) {
    int traffic_join_error = pthread_join(traffic_threads[i], NULL);
    if (traffic_join_error) { printf("Thread %d join failed with code: %d\n", i, traffic_join_error); exit(2); }
    printf("Thread %d finished\n", i);
  }

  // wait for supply thread to finish
  int supply_join_error = pthread_join(supply_thread, NULL);
  if (supply_join_error) { printf("Supply thread join failed with error code: %d\n", supply_join_error); exit(2); }
  printf("Supply thread finished\n");

  // destroy semaphores
  for (int i = 0; i < 4; i++){
    for (int j = 0; j < 4; j++){
      sem_destroy(&semaphores[i][j]);
    }
  }
}
