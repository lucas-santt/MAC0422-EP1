#ifndef IMESH
#define IMESH

// Padrão do C
#include <stdlib.h>
#include <stdio.h>

// Chamadas de sistema (presentes na manpage syscalls)
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <sys/wait.h>

// Leitura de input
#include <readline/readline.h>
#include <readline/history.h>

// Suportes
#include <string.h>

#define USER_MAX_SIZE 256
#define PATH_MAX_SIZE 4096
#define FILE_BUFFER_MAX_SIZE 4096
#define MAX_ARGS_SIZE 6

void pwd(char* wdir, size_t size);
void date_epoch(long int* sec);
void killPid(char* cmd);
void execFile(char* cmd);

int startswith(char* str, char* pre);

void getUsername(char* username_out);
char* readCommandLine();
void execCommand(char* cmd);

#endif