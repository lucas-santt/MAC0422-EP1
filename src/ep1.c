// Padrão do C
#include <stdio.h>
#include <stdlib.h>

// Suporte
#include <string.h>

#define BUFFER_MAX_SIZE 1024
#define MAX_PROCESS_NUMBER 150

struct process {
    char name[255];
    int deadline;
    int t0;
    int dt;
};

typedef struct process process;

process extractProcess(char* process_line) {
    process p = {0};

    strcpy(p.name, strtok(process_line, " "));
    p.deadline = atoi(strtok(NULL, " "));
    p.t0 = atoi(strtok(NULL, " "));
    p.dt = atoi(strtok(NULL, " "));

    return p;
}

void readTrace(char* trace_path, process* process_list) {
    FILE* trace_ptr;
    char buffer[BUFFER_MAX_SIZE];

    trace_ptr = fopen(trace_path, "r"); // TODO: Tratar erro se o arquivo não existe

    int i = 0;
    while(fgets(buffer, BUFFER_MAX_SIZE, trace_ptr) && i < MAX_PROCESS_NUMBER) {
        printf("%s", buffer);
        process_list[i] = extractProcess(buffer);
        i++;
    }

    fclose(trace_ptr);
}

int main(int arg_count, char* args[]) {
    process p[MAX_PROCESS_NUMBER];

    readTrace(args[1], p);
    
    printf("%s\n", p[0].name);

    return 0;
}