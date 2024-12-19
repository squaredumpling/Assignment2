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

typedef struct{
    int side; 
    int direction;
    int index;
} Lane;

// the mutexes controlling what lanes are available
static pthread_mutex_t lane_block[10];

// the matrix representing the intersetion paths of cars
static bool intersect[10][10] = {
    {1, 0, 0, 1, 1, 0, 1, 0, 1, 1}, // 0
    {0, 1, 0, 1, 1, 0, 0, 1, 0, 0}, // 1
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0}, // 2
    {1, 1, 0, 1, 0, 0, 0, 1, 1, 0}, // 3
    {1, 1, 1, 0, 1, 0, 0, 1, 1, 0}, // 4
    {0, 0, 0, 0, 0, 1, 0, 0, 1, 0}, // 5
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 1}, // 6
    {0, 1, 1, 1, 1, 0, 0, 1, 0, 0}, // 7
    {1, 0, 0, 1, 1, 1, 0, 0, 1, 0}, // 8
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 1}, // 9
};

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

// goes through a row of the matrix to lock the intersecting lanes
void advanced_lock(int index) 
{ 
    for (int i = 0; i < 10; i++) {
        if (intersect[index][i]) {
            printf("%d unlocked %d\n", index, i);
            pthread_mutex_lock(&lane_block[index]);
        }
    }
}

// goes through a row of the matrix to unlock the intersecting lanes
void advanced_unlock(int index) 
{ 
    for (int i = 0; i < 10; i++) {
        if (intersect[index][i]) {
            printf("%d unlocked %d\n", index, i);
            pthread_mutex_unlock(&lane_block[index]);
        }
    }
}

/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
    // convert argument to lane stucture
    Lane lane = *(Lane*)arg;

    // the amount of cars that have passed
    int count = 0;

    // read system time and add the maximum time allowed before timeout
    struct timespec system_time;
    clock_gettime(CLOCK_REALTIME, &system_time);
    system_time.tv_sec += END_TIME;

    // while not all arrivals have been handled keep manging
    while(true) {
        // wait for an arrival using the semaphore or timeout after END_TIME seconds
        int wait_error = sem_timedwait(&semaphores[lane.side][lane.direction], &system_time);
        
        if (wait_error) break;

        // lock the the lanes
        advanced_lock(lane.index);

        // make the traffic light turn green
        printf("Traffic light %d %d turns green at time %d for car %d\n", 
        lane.side, lane.direction, get_time_passed(), curr_arrivals[lane.side][lane.direction][count].id);

        // sleep for CROSS_TIME seconds
        sleep(CROSS_TIME);

        // make the traffic light turn red
        printf("Traffic light %d %d turns red at time %d\n", lane.side, lane.direction, get_time_passed());

        // unlock the lanes
        advanced_unlock(lane.index);
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

    // initialize mutex exit array
    for (int i = 0; i < 10; i++) 
        pthread_mutex_init(&lane_block[i], NULL);

    // start the timer
    start_time();

    // initialize properties of lanes
    Lane lanes[10] = {{1, 0, 0}, {1, 1, 1}, {1, 2, 2}, {2, 0, 3}, {2, 1, 4}, {2, 2, 5}, {2, 3, 6}, {3, 0, 7}, {3, 1, 8}, {3, 2, 9}};

    // create a thread per traffic light that executes manage_light
    pthread_t traffic_threads[10];
    for (int i = 0; i < 10; i++) {
        int traffic_create_error = pthread_create(&traffic_threads[i], NULL, manage_light, &lanes[i]);
        if (traffic_create_error) { printf("Thread creation failed with error code: %d\n", traffic_create_error); exit(1); }
        //printf("Thread %d %d created\n", lanes[i].side, lanes[i].direction);
    }

    // create a thread that executes supply_arrivals
    pthread_t supply_thread;
    int supply_create_error = pthread_create(&supply_thread, NULL, supply_arrivals, NULL);
    if (supply_create_error) { printf("Supply thread creation failed with code: %d\n", supply_create_error); exit(1); }
    //printf("Supply thread created\n");

    // wait for all traffic light threads to finish
    for (int i = 0; i < 10; i++) {
        int traffic_join_error = pthread_join(traffic_threads[i], NULL);
        if (traffic_join_error) { printf("Thread %d %d join failed with code: %d\n", lanes[i].side, lanes[i].direction, traffic_join_error); exit(2); }
        // printf("Thread %d %d finished\n", lanes[i].side, lanes[i].direction);
    }

    // wait for supply thread to finish
    int supply_join_error = pthread_join(supply_thread, NULL);
    if (supply_join_error) { printf("Supply thread join failed with error code: %d\n", supply_join_error); exit(2); }
    // printf("Supply thread finished\n");

    // destroy semaphores
    for (int i = 0; i < 4; i++){
        for (int j = 0; j < 4; j++){
        sem_destroy(&semaphores[i][j]);
        }
    }
}
