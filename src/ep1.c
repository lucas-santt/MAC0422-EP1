#define _GNU_SOURCE // Para alocamento da cpu

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
#define RR_QUANTUM_SEC 1

typedef struct process {
    char name[255];
    long int deadline;
    long int t0;
    long int dt;

    long int remaining_dt;
    int priority;
    long int start_time;
    long int finished_time;
} process;

typedef struct Thread {
    int idx;
    pthread_t t;
    process* ps;
    int active;
} Thread;

// Variáveis globais

long int start_time = 0;

process ps_table[MAX_PROCESS_NUMBER];
int ps_count = 0;
int ps_completed = 0;

process* queue[MAX_PROCESS_NUMBER];
int q_count = 0;

long int secToNano(long int sec) {
    return sec * 1000000000L;
}

long int nanoToSec(long int nano) {
    return (nano / 1000000000L);
}

long int nanoToSecRound(long int nano) {
    /* Utilizada para ter mais precisão na hora de imprimir tempo */
    return (long int)round((double)nano / 1000000000.0);
}

long int getCurrTime() {
    /* 
        Retorna o running time do programa em nanosegundos, 
            apenas inputs do usuário estão em segundos.
    */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return secToNano(ts.tv_sec) + ts.tv_nsec - start_time;
}

// Gerenciamento da Fila

void enqueue(process* ps) {
    /* Adiciona um processo na fila */
    if(q_count >= MAX_PROCESS_NUMBER)
        return;

    queue[q_count] = ps;
    q_count++;
}

void dequeue(int idx) {
    if(idx >= q_count || q_count == 0)
        return;

    for(int i = idx; i < q_count - 1; i++) {
        queue[i] = queue[i+1];
    }

    q_count--;
}

void manageQueue(long int now) {
    now = nanoToSec(now);
    /* Gerencia a fila adicionando os processos de acordo com a sua chegada */
    for(int i = 0; i < ps_count; i++){
        if(now < ps_table[i].t0 || ps_table[i].t0 == -1)
            continue;

        printf("%s arrived at queue! (t0=%ld)\n", ps_table[i].name, ps_table[i].t0);
        ps_table[i].t0 = -1;
        enqueue(&ps_table[i]);
    }
}

// Process / Thread Management

int calcPriority(long int deadline, long int now) {
    /* Calcula a prioridade de um processo com base em sua
            deadline e o instante atual de tempo.

        O valo da prioridade varia de 1 a 40, com 39 niveis
            em ordem decrescente, ou seja, valor ↓↑ prioridade.
        A cada segundo mais longe da deadline, o valor da prioridade aumenta

        Inspirada na prioridade do próprio GNU/Linux

        Parâmetros:
            deadline: Deadline do processo em segundos
            now: Instante atual de tempo em segundos
    */
   long int time_to_deadline = deadline - nanoToSec(now);

   if(time_to_deadline <= 0 || time_to_deadline >= 40) return 40;
   return time_to_deadline;
}

long int priorityQuantumSum(int priority) {
    /* 
        Calcula o valor a ser adicionado no quantum 
            com base em sua prioridade, em nanosegundos.
        Valores com menor prioridade recebem mais tempo de quantum.
        Adicionamos de 0 ao quantum, com o processo de menor 
            prioridade tendo o dobro do tempo do quantum.
        Lembrando que sempre truncamos o valor para segundos !!!
    */
   float percent = (40.0 - (float)priority) / 39.0;

   if(RR_QUANTUM_SEC <= 2) {
        // Quando o quantum é muito baixo, aumentamos mais o bonus
        percent = percent * 1.5;
   }

   long int bonus_sec = (long int)round(percent * RR_QUANTUM_SEC);

   return secToNano(bonus_sec);
} 

void onThreadFinished(Thread* curr_t, long int now) {
    process* curr_ps = curr_t->ps;
    now = nanoToSecRound(now);
    
    curr_t->active = 0;
    if(curr_ps->remaining_dt > 0) {
        // Processo foi preemptado
        printf("%s preempted at time %ld\n",
            curr_ps->name, nanoToSecRound(curr_ps->finished_time));
        enqueue(curr_ps);
    } else {
        // Processo foi finalizado
        printf("%s finished at time %ld\n", 
            curr_ps->name, nanoToSecRound(curr_ps->finished_time));
        ps_completed++;
        curr_t->ps = NULL;
    }
}

void onThreadPreempted(void* arg) {
    process* ps = (process*)arg;
    long int now = getCurrTime();

    ps->finished_time = now;
}

void* worker(void* arg) {
    /*
        Simula a execução de um processo.

        Obs.: Cancelation type deve ser deferred pois, assim, controlamos o momento da preempção
            do processo (no cancelation point).
    */
    process* ps = (process*)arg;
    long int now = getCurrTime();
    long int finish_time = now + secToNano(ps->remaining_dt);
    ps->start_time = now;
    pthread_cleanup_push(onThreadPreempted, ps);

    // Simulação da execução do processo
    while(now < finish_time) {
        now = getCurrTime();
        ps->remaining_dt = nanoToSecRound(finish_time - now);
        pthread_testcancel(); // Único cancelation point da execução do ps
    }

    // Já acabamos! Para evitar ações duplicados, n precisa mais realizar preempção
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(0);
    
    ps->remaining_dt = 0;
    ps->finished_time = now;    
    return NULL;
}

// Escalonadores

int getShortestProcess(process* out_process){
    /* Retorna o processo de menor duração e o retira da fila */
    if(q_count == 0) return -1;
    
    int shortest_ps_idx = -1;  
    for(int i = 0; i < q_count; i++) {
        if(shortest_ps_idx == -1 || queue[i]->dt < queue[shortest_ps_idx]->dt)
            shortest_ps_idx = i;
    }

    if(shortest_ps_idx == -1) return -1;
    out_process = queue[shortest_ps_idx];
    dequeue(shortest_ps_idx);
    return shortest_ps_idx;
}

int getNextProcess(process** out_process) {
    /* Retorna o próximo processo da fila e o retira da fila */
    if(q_count == 0) return 0;

    *out_process = queue[0];
    dequeue(0);
    return 1;
}

int getNextPriorityProcess(process** out_process, int currPriority) {
    /* 
        Adiciona o próximo processo com prioridade 
            menor que a atual no out_process
        Retorna 1 caso tenha encontrado um processo, 0 caso contrário
    */
    if(q_count == 0) return 0;

    int lowest_priority = currPriority;
    int lowest_ps_idx = -1;
    for(int i = 0; i < q_count; i++) {
        if(queue[i]->priority < lowest_priority) {
            lowest_ps_idx = i;
            lowest_priority = queue[i]->priority;
        }
    }

    if(lowest_ps_idx == -1) return 0;

    *out_process = queue[lowest_ps_idx];
    dequeue(lowest_ps_idx);
    return 1;
}

void sjf() {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN) - 1; // Um thread para o gerenciado da fila;
    if(num_cpus <= 0) num_cpus = 1;

    int num_threads = ps_count < num_cpus ? ps_count : num_cpus;

    pthread_t threads[num_threads];
    int active[num_threads];
    cpu_set_t cpuset;

    for(int i=0; i<num_threads; i++) 
        active[i] = 0; // Inativo
        
    int completed = 0;
    while(completed < ps_count) {
        for(int i=0; i<num_threads; i++) {
            if(active[i]) {
                // Verifica se a thread já terminou
                if(pthread_tryjoin_np(threads[i], NULL) == 0) {
                    active[i] = 0;
                    completed++;
                } else {
                    continue;
                }
            }
            // Thread inativa, tenta pegar um processo para rodar
            process* next_ps = malloc(sizeof(process));
            
            int has_process = getShortestProcess(next_ps);

            if(!has_process) {
                free(next_ps);
                usleep(1000); // Para não gastar recurso em um loop rápido
                continue;
            }

            pthread_create(&threads[i], NULL, worker, next_ps);
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);

            pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset);

            active[i] = 1;
            free(next_ps);
        }   
    }
}

void roundRobin() {
    /*
        Escalonador Round Robin, onde cada processo possui um quantum 
            de tempo para execução. Sempre seguindo first in first out.

        Cada thread i possui três propriedades:
            active[i]: 0 - Inativo, 1 - Ativo, 2 - Em preempção
                Foi necessário adicionar o estado 2 para evitar ações 
                duplicadas do próprio esalonador
            last_start_time[i]: Tempo de início da execução do processo
            running_processes[i]: Processo que está sendo executado
    */
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int num_threads = ps_count < num_cpus ? ps_count : num_cpus;
    cpu_set_t cpuset;

    pthread_t threads[num_threads];
    int active[num_threads];
    long int last_start_time[num_threads];
    process* running_processes[num_threads];

    for(int i=0; i<num_threads; i++) {
        active[i] = 0; // Inativo
        last_start_time[i] = 0;
        running_processes[i] = NULL;
    }
        
    while(ps_completed < ps_count) {
        long int now = getCurrTime(false);
        manageQueue(now);

        for(int i=0; i<num_threads; i++) {
            if(active[i]) {
                // Verifica se a thread já terminou
                if(pthread_tryjoin_np(threads[i], NULL) == 0) {
                    active[i] = 0;
                    if(running_processes[i]->remaining_dt > 0) {
                        // Processo foi preemptado
                        printf("%s preempted at time %ld\n", 
                            running_processes[i]->name, now);
                        enqueue(running_processes[i]);
                    } else {
                        // Processo foi finalizado
                        printf("%s finished at time %ld\n", 
                            running_processes[i]->name, now);
                        ps_completed++;
                    }
                } else {
                    // Verifica se o quantum já passou
                    if(active[i] == 1 && now - last_start_time[i] >= RR_QUANTUM_SEC) {
                        pthread_cancel(threads[i]);
                        active[i] = 2;
                    }
                    continue;
                }
            }
            // Thread inativa, tenta pegar um processo para rodar
            process* next_ps;
            int has_process = getNextProcess(&next_ps);
            if(!has_process) {
                usleep(1000); // Para não gastar recurso em um loop rápido
                continue;
            }

            printf("%s started at time %ld\n", 
                next_ps->name, now);

            active[i] = 1;
            running_processes[i] = next_ps;
            last_start_time[i] = now;
            pthread_create(&threads[i], NULL, worker, next_ps);
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset);

        }   
    }
}

void singlePriorityThread(Thread* curr_t, bool isRR, long int now) {
    /*
        Cada thread possui duas propriedades:
            active: 0 - Inativo, 1 - Ativo, 2 - Em preempção
                Foi necessário adicionar o estado 2 para evitar ações 
                duplicadas do próprio esalonador
            start_time: Tempo de início da execução do último processo
    */
    process* curr_ps = curr_t->ps;
    cpu_set_t cpuset;

    if(curr_t->active) {
        // Verifica se a thread já terminou
        if(pthread_tryjoin_np(curr_t->t, NULL) == 0) {
            onThreadFinished(curr_t, now);
        } else {
            // Verifica se o quantum já passou
            long int quantum = secToNano(RR_QUANTUM_SEC) + priorityQuantumSum(curr_ps->priority);
            long int elapsed_time = now - curr_ps->start_time;
            if(curr_t->active == 1 && elapsed_time >= quantum) {
                pthread_cancel(curr_t->t);
                curr_t->active = 2;
            }
        }
        return;
    }

    // Thread inativa, tenta pegar um processo para rodar
    process* next_ps;
    int has_process;

    if(isRR) 
        has_process = getNextProcess(&next_ps);
    else {
        int curr_priority = 41;
        if(curr_ps != NULL && curr_ps->remaining_dt > 0)
            curr_priority = curr_ps->priority;
        has_process = getNextPriorityProcess(&next_ps, curr_priority);
    }

    if(!has_process) {
        usleep(1000); // Para não gastar recurso em um loop rápido
        return;
    }

    printf("%s started at time %ld (priority: %d)\n", 
        next_ps->name, nanoToSec(now), next_ps->priority);

    curr_t->active = 1;
    curr_t->ps = next_ps;
    next_ps->start_time = now;

    pthread_create(&curr_t->t, NULL, worker, next_ps);
    CPU_ZERO(&cpuset);
    CPU_SET(curr_t->idx, &cpuset);
    pthread_setaffinity_np(curr_t->t, sizeof(cpu_set_t), &cpuset);
}

void escPrioridade(bool isRR) {
    /*
        Round Robin

        Escalonador com prioridade, onde cada processo possui
            uma prioridade definida a partir da deadline e o
            atual instante de tempo.
    */
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int num_threads = ps_count < num_cpus ? ps_count : num_cpus;

    Thread* t_buffer[num_threads];

    for(int i=0; i<num_threads; i++) {
        t_buffer[i] = malloc(sizeof(Thread));
        t_buffer[i]->idx = i;
        t_buffer[i]->ps = NULL;
        t_buffer[i]->active = 0;
    }
        
    while(ps_completed < ps_count) {
        long int now = getCurrTime();
        manageQueue(now);

        if(!isRR) {
            for(int i=0; i<q_count; i++) {
                queue[i]->priority = calcPriority(queue[i]->deadline, now);
            }
        }

        for(int i=0; i<num_threads; i++) {
            singlePriorityThread(t_buffer[i], isRR, now);
        }   
    }

    for(int i=0; i<num_threads; i++)
        free(t_buffer[i]);
}

// Funções inicializadoras

process extractProcess(char* process_line) {
    process ps;

    strcpy(ps.name, strtok(process_line, " "));
    ps.deadline = atoi(strtok(NULL, " "));
    ps.t0 = atoi(strtok(NULL, " "));
    ps.dt = atoi(strtok(NULL, " "));
    ps.remaining_dt = ps.dt;
    ps.priority = 1;

    return ps;
}

void readTrace(char* trace_path) {
    FILE* trace_ptr;
    char buffer[BUFFER_MAX_SIZE];

    trace_ptr = fopen(trace_path, "r"); // TODO: Tratar erro se o arquivo não existe

    int i = 0;
    while(fgets(buffer, BUFFER_MAX_SIZE, trace_ptr) && i < MAX_PROCESS_NUMBER) {
        ps_table[i] = extractProcess(buffer);
        i++;
    }
    ps_count = i;
    fclose(trace_ptr);
}

int main(int arg_count, char* args[]) {    
    start_time = getCurrTime();
    readTrace(args[1]);
    
    escPrioridade(false);

    return 0;
}