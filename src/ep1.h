#ifndef EP1
#define EP1

#define _GNU_SOURCE

// Padrão do C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Pthreads
#include <pthread.h>
#include <sched.h>

// Suporte
#include <string.h>
#include <unistd.h>
#include <math.h>

#define BUFFER_MAX_SIZE 1024
#define MAX_PROCESS_NUMBER 150
#define QUANTUM_SEC 1

typedef struct process {
    char name[255];
    long int deadline;
    long int t0;
    long int dt;

    int arrived;

    long int remaining_dt;
    int priority;

    long int finished_time;
} process;

typedef struct Thread {
    int idx;
    pthread_t t;
    process* ps;
    int active;
    long int start_time;
    long int quantum;
} Thread;


extern long int start_time;
extern process ps_table[MAX_PROCESS_NUMBER];
extern int ps_count;
extern int ps_completed;
extern int preemptions;
extern process* queue[MAX_PROCESS_NUMBER];
extern int q_count;

long int secToNano(long int sec);
long int nanoToSec(long int nano);
float nanoToSecFormatted(long int nano);
long int getCurrTime();

void enqueue(process* ps);
void dequeue(int idx);
void manageQueue(long int now);

int calcPriority(process* ps, long int now);
long int priorityToQuantum(int priority);
void onThreadFinished(Thread* curr_t);
void* worker(void* arg);

int getShortestProcess(process** out_process);
int getNextProcess(process** out_process);
int getNextPriorityProcess(process** out_process, int currPriority);

void singleSjfThread(Thread* curr_t, long int now);
void sjf();
void singlePriorityThread(Thread* curr_t, bool isRR, long int now);
void escPrioridade(bool isRR);

process extractProcess(char* process_line);
void readTrace(char* trace_path);
void writeOutput(char* output_path);

#endif