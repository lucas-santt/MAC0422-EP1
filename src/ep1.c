# include "ep1.h"

// Variáveis globais

long int start_time = 0;

process ps_table[MAX_PROCESS_NUMBER];
int ps_count = 0;
int ps_completed = 0;
int preemptions = 0;

process* queue[MAX_PROCESS_NUMBER];
int q_count = 0;

long int secToNano(long int sec) {
    return sec * 1000000000L;
}

long int nanoToSec(long int nano) {
    return (nano / 1000000000L);
}

float nanoToSecFormatted(long int nano) {
    /* Formatted for visualization, mas pode perder precisão */
    return (float)nano / 1000000000.0;
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
    /* Gerencia a fila adicionando os processos de acordo com a sua chegada */
    now = nanoToSec(now);
    for(int i = 0; i < ps_count; i++){
        if(now < ps_table[i].t0 || ps_table[i].arrived == 1)
            continue;

        printf("%s arrived at queue! (t0=%ld)\n", ps_table[i].name, ps_table[i].t0);
        ps_table[i].arrived = 1;
        ps_table[i].t0 = getCurrTime();
        enqueue(&ps_table[i]);
    }
}

// Process / Thread Management

int calcPriority(process* ps, long int now) {
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
   long int time_to_deadline = ps->deadline - nanoToSec(now);

   if(time_to_deadline <= 0 || time_to_deadline > ps->remaining_dt || time_to_deadline >= 40) return 40;
   return time_to_deadline;
}

long int priorityQuantum(int priority) {
    /* 
        Calcula o valor a ser adicionado no quantum 
            com base em sua prioridade, em nanosegundos.
        Valores com menor prioridade recebem mais tempo de quantum.
        Adicionamos de 0 ao quantum, com o processo de menor 
            prioridade tendo o dobro do tempo do quantum.
        Lembrando que sempre truncamos o valor para segundos !!!
    */
   float percent = (40.0 - (float)priority) / 39.0;

   // Como não podemos fazer secToNano(percent * QUANTUM_SEC), temos que multiplicar manualmente
   long int bonus_nanosec = (long int)(percent * QUANTUM_SEC * 1000000000L);

   return secToNano(QUANTUM_SEC) + bonus_nanosec;
} 

void onThreadFinished(Thread* curr_t) {
    process* curr_ps = curr_t->ps;
    
    curr_t->active = 0;
    if(curr_ps->remaining_dt > 0) {
        // Processo foi preemptado
        printf("%s preempted at time %f\n",
            curr_ps->name, nanoToSecFormatted(curr_ps->finished_time));
        enqueue(curr_ps);
    } else {
        // Processo foi finalizado
        printf("%s finished at time %f\n", 
            curr_ps->name, nanoToSecFormatted(curr_ps->finished_time));
        ps_completed++;
    }
    curr_t->ps = NULL;
}

void* worker(void* arg) {
    /*
        Simula a execução de um processo.

        Obs.: Cancelation type deve ser deferred pois, assim, controlamos o momento da preempção
            do processo (no cancelation point).
    */
    process* ps = (process*)arg;
    long int now = getCurrTime();
    long int finish_time = now + ps->remaining_dt;

    // Simulação da execução do processo
    while(now < finish_time) {
        now = getCurrTime();
        ps->finished_time = now;
        pthread_testcancel(); // Único cancelation point da execução do ps
    }
    
    ps->remaining_dt = 0;
    ps->finished_time = now;    
    return NULL;
}

// Escalonadores

int getShortestProcess(process** out_process){
    /* Retorna o processo de menor duração e o retira da fila */
    if(q_count == 0) return 0;
    
    int shortest_ps_idx = -1;  
    for(int i = 0; i < q_count; i++) {
        if(shortest_ps_idx == -1 || queue[i]->dt < queue[shortest_ps_idx]->dt)
            shortest_ps_idx = i;
    }

    if(shortest_ps_idx == -1) return 0;
    *out_process = queue[shortest_ps_idx];
    dequeue(shortest_ps_idx);
    return 1;
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

void singleSjfThread(Thread* curr_t, long int now) {
    if(curr_t->active) {
        // Verifica se a thread já terminou
        if(pthread_tryjoin_np(curr_t->t, NULL) != 0) 
            return;

        printf("%s finished at time %f\n", 
            curr_t->ps->name, nanoToSecFormatted(curr_t->ps->finished_time));

        curr_t->active = 0;
        ps_completed++;

        return;
    }
    // Thread inativa, tenta pegar um processo para rodar
    process* next_ps;
    cpu_set_t cpuset;

    
    int has_process = getShortestProcess(&next_ps);
    if(!has_process) {
        //usleep(1000); // Para não gastar recurso em um loop rápido
        return;
    }

    printf("%s started at time %f\n", next_ps->name, nanoToSecFormatted(now));

    curr_t->ps = next_ps;
    pthread_create(&curr_t->t, NULL, worker, next_ps);
    CPU_ZERO(&cpuset);
    CPU_SET(curr_t->idx, &cpuset);
    pthread_setaffinity_np(curr_t->t, sizeof(cpu_set_t), &cpuset);
    curr_t->active = 1;
}

void sjf() {
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
        
        for(int i=0; i<num_threads; i++) {
            long int now = getCurrTime();
            manageQueue(now);
            singleSjfThread(t_buffer[i], now);
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

    if(curr_t->active) {
        void* status;
        // Verifica se a thread já terminou
        if(pthread_tryjoin_np(curr_t->t, &status) == 0) {
            if(status == PTHREAD_CANCELED) 
                curr_ps->remaining_dt -= curr_t->quantum;
            else
                curr_ps->remaining_dt = 0;
            
            onThreadFinished(curr_t);
        } else {
            // Verifica se o quantum já passou
            long int elapsed_time = now - curr_t->start_time;
            if(curr_t->active == 1 && elapsed_time >= curr_t->quantum) {
                pthread_cancel(curr_t->t);
                curr_t->active = 2;
                preemptions++;
            }
        }
        return;
    }

    // Thread inativa, tenta pegar um processo para rodar
    process* next_ps;
    cpu_set_t cpuset;
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

    long int quantum = priorityQuantum(next_ps->priority);

    if(isRR)
        printf("%s started at time %f\n", next_ps->name, nanoToSecFormatted(now));
    else
        printf("%s started at time %f (priority: %d - quantum: %f)\n", 
            next_ps->name, nanoToSecFormatted(now), next_ps->priority, nanoToSecFormatted(quantum));

    curr_t->active = 1;
    curr_t->ps = next_ps;
    curr_t->start_time = now;
    curr_t->quantum = quantum;

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
                queue[i]->priority = calcPriority(queue[i], now);
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
    ps.arrived = 0;
    ps.t0 = atoi(strtok(NULL, " "));
    ps.dt = atoi(strtok(NULL, " "));
    ps.remaining_dt = secToNano(ps.dt);
    ps.priority = 40;

    return ps;
}

void readTrace(char* trace_path) {
    FILE* trace_ptr;
    char buffer[BUFFER_MAX_SIZE];

    trace_ptr = fopen(trace_path, "r"); 

    if(trace_ptr == NULL) {
        printf("Error opening trace file!\n");
        exit(1);
    }

    int i = 0;
    while(fgets(buffer, BUFFER_MAX_SIZE, trace_ptr) && i < MAX_PROCESS_NUMBER) {
        ps_table[i] = extractProcess(buffer);
        i++;
    }
    ps_count = i;
    fclose(trace_ptr);
}

void writeOutput(char* output_path) {
    FILE* output_ptr = fopen(output_path, "w");
    
    if(output_ptr == NULL) {
        printf("Error opening output file!\n");
        return;
    }

    for(int i = 0; i < ps_count; i++) {
        process ps = ps_table[i];

        int completed = 0;
        if(ps.finished_time <= secToNano(ps.deadline)) completed = 1;

        float tf = nanoToSecFormatted(ps.finished_time);
        long int tr = ps.finished_time - ps.t0;

        fprintf(output_ptr, "%d %s %f %f\n", completed, ps.name, tf, nanoToSecFormatted(tr));
    }
    fprintf(output_ptr, "%d\n", preemptions);
    fclose(output_ptr);
}

int main(int arg_count, char* args[]) {    
    start_time = getCurrTime();
    readTrace(args[2]);

    int esc = atoi(args[1]);

    if(esc == 1)
        sjf();
    else if(esc == 2)
        escPrioridade(true);
    else
        escPrioridade(false);

    if(args[3] != NULL)
        writeOutput(args[3]);

    return 0;
}