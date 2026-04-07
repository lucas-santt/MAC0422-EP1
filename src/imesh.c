#include "imesh.h"

// Comandos (chamadas de sistema)

void pwd(char* wdir, size_t size) {
    getcwd(wdir, size); // syscall gethostname(2)
}

void date_epoch(long int* sec) {
    /* Equivalente ao 'date +%s' */
    struct timespec t;

    clock_gettime(CLOCK_REALTIME, &t); // syscall clock_gettime(2)

    *sec = t.tv_sec;
}

void killPid(char* cmd) {
    /* Recebe o comando kill mata um processo específico */
    pid_t pid;
    int sig;
    // Sabemos que teremos apenas 2 argumentos, mas por 
    // questão de consistência, irei deixar como MAX_ARGS_SIZE
    char* args[MAX_ARGS_SIZE];

    args[0] = strtok(cmd + 6, " "); // pula o "kill -"
    args[1] = strtok(NULL, " ");

    sig = atoi(args[0]);
    pid = (pid_t) atol(args[1]);

    int res = kill(pid, sig); // syscall kill(2)

    if(res == -1) {
        printf("imesh: kill: (%s) - ", args[1]);
        // syscall errno(3)
        if(errno == ESRCH) {
            printf("No such process");
        } else if(errno == EPERM) {
            printf("Operation not permitted");
        }
        printf("\n");
    }
}

void execFile(char* cmd) {
    /* Executa um arquivo específico com seus argumentos */
    char* args[MAX_ARGS_SIZE];

    // Primeiro, vamos extrair os argumentos
    int i = 0;
    char* temp_str = strtok(cmd, " ");
    while(temp_str) {
        args[i] = temp_str;
        temp_str = strtok(NULL, " ");
        i++;
    }
    args[i] = NULL; // Precisa ser NULL para o execv (li na man page)


    if(fork() != 0) {
        int status;
        waitpid(-1, &status, 0);
    } else {
        execv(args[0], args);
    }
}

// Funções suportes

int startswith(char* str, char* pre) {
    /* Retrona True se str começa com pre, False caso contrário*/
    return !strncmp(pre, str, strlen(pre));
}

// Funções do terminal em si

void getUsername(char* username_out) {
    /*
        Escreve o nome de usuário na variável username_out, não
            escreve nada caso contrário.
    */
    struct passwd* pwd = getpwuid(geteuid());
    if (pwd) {
        strcpy(username_out, pwd->pw_name);
    } else {
        strcpy(username_out, "");
    }
}

char* readCommandLine() {
    /* Exibe o prompt do shell e retorna a resposta do usuário */

    char username[USER_MAX_SIZE];
    char hostname[USER_MAX_SIZE];
    char working_dir[PATH_MAX_SIZE];

    getUsername(username);
    gethostname(hostname, sizeof(hostname)); // syscall gethostname(2)
    pwd(working_dir, sizeof(working_dir));

    char prompt[USER_MAX_SIZE * 2 + PATH_MAX_SIZE + 10];
    snprintf(prompt, sizeof(prompt), "[%s@%s:%s]$ ", username, hostname, working_dir);

    char* input = readline(prompt); 

    if(input && *input)
        add_history(input);

    return input;
}

void execCommand(char* cmd){
    /*
        Executa o comando passado pelo
            parâmetro cmd.

        Apenas suporta um número limitado 
            de comandos, por hora.
    */

    // P.S: strcmp retorna 0 quando são iguais
    if(!strcmp(cmd, "pwd")) {
        char path[PATH_MAX_SIZE];

        pwd(path, sizeof(path));
        printf("%s\n", path);
    } else if(!strcmp(cmd, "date +%s")) {
        long int time_since;
        
        date_epoch(&time_since);
        printf("%ld\n", time_since);
    } else if(startswith(cmd, "kill")) {
        killPid(cmd);
    } else if(startswith(cmd, "/") || startswith(cmd, "./")) {
        execFile(cmd);
    }

    exit(0);
}

int main(){
    using_history();

    while(1) {
        char* input; // Documentação fala para dar free depois
        input = readCommandLine();

        // Se fizer isso dentro de execCommand ele
        // sairía apenas do processo filho
        if(!strcmp(input, "exit")) {
            free(input);
            exit(0); // syscall exit(2)
        }

        if(fork() != 0) {
            int status;
            waitpid(-1, &status, 0); // syscall waitpid(2)
        } else {
            execCommand(input);
        }

        free(input);
    }
}