/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 *
 * rq is a pointer to a struct you should use for your ready queue
 * implementation. The head of the queue corresponds to the process
 * that is about to be scheduled onto the CPU, and the tail is for
 * convenience in the enqueue function. See student.h for the 
 * relevant function and struct declarations.
 *
 * Similar to current[], rq is accessed by multiple threads,
 * so you will need to use a mutex to protect it. ready_mutex has been
 * provided for that purpose.
 *
 * The condition variable queue_not_empty has been provided for you
 * to use in conditional waits and signals.
 *
 * Please look up documentation on how to properly use pthread_mutex_t
 * and pthread_cond_t.
 *
 * A scheduler_algorithm variable and sched_algorithm_t enum have also been
 * supplied to you to keep track of your scheduler's current scheduling
 * algorithm. You should update this variable according to the program's
 * command-line arguments. Read student.h for the definitions of this type.
 */
static pcb_t **current;
static queue_t *rq;

static pthread_mutex_t current_mutex;
static pthread_mutex_t queue_mutex;
static pthread_cond_t queue_not_empty;

static sched_algorithm_t scheduler_algorithm;
static unsigned int cpu_count;
int timeslice;

/*
 * enqueue() is a helper function to add a process to the ready queue.
 *
 * This function should add the process to the ready queue at the
 * appropriate location.
 *
 * NOTE: For SJF and SRTF scheduling, you will need to have additional logic
 * in either this function or the dequeue function to make the ready queue
 * a priority queue.
 */
void enqueue(queue_t *queue, pcb_t *process) {
    /* FIX ME */
    /* FIX ME */
    pthread_mutex_lock(&queue_mutex);
    pcb_t *curr_tail = queue->tail;
    if (curr_tail == NULL) {
        queue->head = process;
        queue->tail = process;
    } else if (scheduler_algorithm == SJF || scheduler_algorithm == SRTF) {
        if (process -> time_remaining < queue -> head ->time_remaining) {
            process -> next = queue -> head;
            queue -> head = process;
        } else {
            pcb_t *node = queue -> head;
            pcb_t *prev = NULL;
            while (node != NULL && node->time_remaining <= process -> time_remaining) {
                prev = node;
                node = node->next;
            }
            if (node == NULL) {
                node = queue->tail;
                node -> next = process;
                process -> next = NULL;
                queue -> tail = process; 
            } else {
                prev -> next = process;
                process -> next = node;
            }
        }    
    } else {
        curr_tail -> next = process;
        queue -> tail = curr_tail -> next;                                                      
        queue -> tail -> next = NULL;
    }
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
}

/*
 * dequeue() is a helper function to remove a process to the ready queue.
 *
 * This function should remove the process at the head of the ready queue
 * and return a pointer to that process. If the queue is empty, simply return
 * NULL.
 *
 * NOTE: For SJF and SRTF scheduling, you will need to have additional logic
 * in either this function or the enqueue function to make the ready queue
 * a priority queue.
 */
pcb_t *dequeue(queue_t *queue) {
    /* FIX ME */
    pcb_t *ret_val;
    pthread_mutex_lock(&queue_mutex);
    if (is_empty(queue)) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    } else { 
        ret_val = queue->head;
        queue->head = queue->head->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        ret_val -> next = NULL;
        pthread_mutex_unlock(&queue_mutex);
        return ret_val;
    }
}

/*
 * is_empty() is a helper function that returns whether the ready queue
 * has any processes in it.
 *
 * If the ready queue has no processes in it, is_empty() should return true.
 * Otherwise, return false.
 */
bool is_empty(queue_t *queue) {
    /* FIX ME */
    return queue -> head == NULL;
}

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which
 *  you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Set the currently running process using the current array
 *
 *   4. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *
 *  The current array (see above) is how you access the currently running process indexed by the cpu id.
 *  See above for full description.
 *  context_switch() is prototyped in os-sim.h. Look there for more information
 *  about it and its parameters.
 */
static void schedule(unsigned int cpu_id) {
    /* FIX ME */
    //pthread_mutex_lock(&queue_mutex);
    pcb_t* process = dequeue(rq);
    //pthread_mutex_unlock(&queue_mutex);
    if (process == NULL) {
        context_switch(cpu_id, 0, -1);
    } else {
        process -> state = PROCESS_RUNNING;
        pthread_mutex_lock(&current_mutex);
        current[cpu_id] = process;
        pthread_mutex_unlock(&current_mutex);
        context_switch(cpu_id, process, timeslice);
    }
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id) {
    /* FIX ME */
    pthread_mutex_lock(&queue_mutex);
    while(is_empty(rq)) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }
    pthread_mutex_unlock(&queue_mutex);
    schedule(cpu_id);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring in the case of Round Robin
 * scheduling or if a process with a lower remaining time is ready in the case of
 * SRTF scheduling.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * Remember to set the status of the process to the proper value.
 */
extern void preempt(unsigned int cpu_id) {
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    pcb_t* process = current[cpu_id];
    pthread_mutex_unlock(&current_mutex);
    //pthread_mutex_lock(&queue_mutex);
    process -> state = PROCESS_READY;
    enqueue(rq, process);
    //pthread_mutex_unlock(&queue_mutex);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id) {
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    pcb_t* process = current[cpu_id];
    process -> state = PROCESS_WAITING;
    //current[cpu_id] = NULL;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id) {
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    pcb_t* process = current[cpu_id];
    //current[cpu_id] = NULL;
    process -> state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is SRTF, wake_up() may need
 *      to preempt the process that is currently executing on the CPU to allow it to
 *      execute the process which just woke up.  To select a processor for SRTF
 *      preemption, iterate through current[] and find the one with the longest remaining
 *      time (that's longer than the waking process).  However, if ANY CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with an equal or lower remaining time than the one which just woke up,
 *      wake_up() should NOT preempt any CPUs.
 *
 *  To preempt a process, use force_preempt(). Look in os-sim.h for
 *  its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process) {
    /* FIX ME */
    process-> state = PROCESS_READY;
    enqueue(rq, process);
    //pthread_mutex_lock(&queue_mutex);
    if (scheduler_algorithm == SRTF) {
        pthread_mutex_lock(&current_mutex);         
        unsigned int time = process -> time_remaining;
        int id = -1;
        for (int i = 0; i < cpu_count; i++) {
            if (current[i] == NULL) {
                pthread_mutex_unlock(&current_mutex);
                return;
            }
            if (current[i] -> time_remaining > time ) {
                time = current[i] -> time_remaining;
                id = i;
            }
        }
        pthread_mutex_unlock(&current_mutex);
        if (id != -1) {
            force_preempt(id);
        }
    }
    //pthread_mutex_unlock(&queue_mutex);
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -j, -r, and -s command-line parameters.
 */
int main(int argc, char *argv[]) {
    /*
     * Check here if the number of arguments provided is valid.
     * You will need to modify this when you add more arguments.
     */
    if (argc > 4 || argc < 2) {                                       
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                        "Usage: ./os-sim <# CPUs> [ -r <time slice> | -j | -s ]\n"
                        "    Default : FCFS Scheduler\n"
                       "         -j : Shortest Job First Scheduler\n"
                        "         -r : Round-Robin Scheduler\n"
                        "         -s : Shortest Remaining Time First Scheduler (Extra Credit)\n\n");
        return -1;
    }
    /*
     * FIX ME - Add support for -j, -r, and -s parameters.
     *
     * Update scheduler_algorithm (see student.h) from the program arguments. If
     * no argument has been supplied, you should default to FCFS.  Use the
     * scheduler_algorithm variable in your scheduler to keep track of the scheduling
     * algorithm you're using.
     */
    if (argc == 4) {
        if ((strcmp("-r", argv[2]) == 0)) {
            scheduler_algorithm = RR;
            timeslice = strtoul(argv[3], NULL, 0);
        }
    } else if (argc == 3) {
        if (strcmp("-j", argv[2]) == 0) {        
            scheduler_algorithm = SJF;
        } else if ((strcmp("-s", argv[2]) == 0)) {
            scheduler_algorithm = SRTF;
        }
    } else {
        scheduler_algorithm = FCFS;
    }

    /* Parse the command line arguments */
    cpu_count = strtoul(argv[1], NULL, 0);

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t *) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_empty, NULL);
    rq = (queue_t *) malloc(sizeof(queue_t));
    assert(rq != NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}

#pragma GCC diagnostic pop
