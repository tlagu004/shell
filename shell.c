#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>  
#include <sys/stat.h>
#include <signal.h> 
#include <dirent.h>

#define MAX_LINE 80  
#define MAX_ALIASES 10

struct alias {
    char name[20];
    char command[MAX_LINE];
};

struct alias aliases[MAX_ALIASES];
int alias_count = 0;
pid_t bg_jobs[100];
int bg_jobs_count = 0;

// HANDLE SIGNALS (CTRL+C or +Z)
void handle_signals(int sig){
    if (sig == SIGINT){
        printf("\n[SIGINT] Use 'exit' to quit.\nosh> ");
    } else if (sig == SIGTSTP){
        printf("\n[SIGTSTP] Shell suspension disabled.\nosh> ");
    }
    fflush(stdout);
}

// MONITORS JOBS/TASKS
void monitor_jobs(){
    for (int j = 0; j < bg_jobs_count; j++){
        if (bg_jobs[j] > 0 && waitpid(bg_jobs[j], NULL, WNOHANG) > 0){
            printf("\n[Background job %d completed]\nosh> ", bg_jobs[j]);
            fflush(stdout);
            bg_jobs[j] = -1;
        }
    }
}

// APPLY ALIASES
void apply_aliases(char *input){
    for (int a = 0; a < alias_count; a++){
        int len = strlen(aliases[a].name);
        if (strncmp(input, aliases[a].name, len) == 0 && (input[len] == ' ' || input[len] == '\0')){
            char temp_input[MAX_LINE];
            sprintf(temp_input, "%s%s", aliases[a].command, input + len);
            strcpy(input, temp_input);
            break;
        }
    }
}

// EXECUTE COMMAND
void execute_command(char **args, int background){
    pid_t pid = fork();
            
    if (pid < 0) { 
        perror("Fork Failed");
    } else if (pid == 0) { // CHILD PROCESS
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], ">") == 0) {
                int fd = open(args[j+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("Error opening file"); exit(1); }
                dup2(fd, STDOUT_FILENO); 
                close(fd);  
                args[j] = NULL;
            } else if (strcmp(args[j], "<") == 0) {
                int fd = open(args[j+1], O_RDONLY);
                if (fd < 0) { perror("Error opening file"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);                        
                args[j] = NULL;
            }
        }
        if (execvp(args[0], args) == -1){
            perror("Regular execution failed");
            exit(1);
        }
    } else { // PARENT PROCESS
        if (background == 0) {
            fprintf(stderr, "Parent waiting for child PID %d...\n", pid);
            waitpid(pid, NULL, 0);
        } else {
            fprintf(stderr, "Parent continuing (Child PID %d in background)\n", pid);
            bg_jobs[bg_jobs_count++] = pid;
        }
    }
}

int main(void)
{
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE] = "";
    char history[MAX_LINE]; 
    int has_history = 0;  
    int should_run = 1; 

    // HANDLE CTRL+C and CTRL+Z
    signal(SIGINT, handle_signals);
    signal(SIGTSTP, handle_signals);

    while (should_run == 1) {
        monitor_jobs();
        printf("osh> ");
        fflush(stdout);
        
        /**
        * After reading user input, the steps are:
        * (1) fork a child process using fork()
        * (2) the child process will invoke execvp()
        * (3) parent will invoke wait() unless command included &
        */
        
        // USER INPUT
        if (fgets(input, MAX_LINE, stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';

        // CHECK EMPTY INPUT
        if (strlen(input) == 0) continue;

        // COMMAND COMPLETION
        if (input[strlen(input) - 1] == '?'){
            input[strlen(input) - 1] = '\0';
            if (has_history && strncmp(history, input, strlen(input))== 0){
                printf("Completion found: %s\n", history);
                strcpy(input, history);
            } else {
                printf("No match in history. Listed Directory Hints:\n");
                DIR *d = opendir(".");
                struct dirent *dir;
                while ((dir = readdir(d)) != NULL) {
                    if (strncmp(dir->d_name, input, strlen(input)) == 0)
                        printf("  %s\n", dir->d_name);
                }
                closedir(d);
                fflush(stdout);
                continue;
            }
        }

        // EXIT 
        if (strcmp(input, "exit") == 0) {
            fprintf(stderr, "(exit detected) Terminating shell...\n");
            should_run = 0;
            continue;
        }
        
        // KILL PROCESS
        if (strncmp(input, "kill ", 5) == 0) {
            pid_t pid_to_kill = atoi(input + 5);
            if (kill(pid_to_kill, SIGKILL) == 0) printf("Process %d terminated.\n", pid_to_kill);
            else perror("Kill failed");
            continue;
        }

        // CREATE ALIAS
        if (strncmp(input, "alias ", 6) == 0){
            char *name = strtok(input + 6, "=");
            char *command = strtok(NULL, "");
            if (name && command && alias_count < MAX_ALIASES){
                strcpy(aliases[alias_count].name, name);
                strcpy(aliases[alias_count].command, command);
                alias_count++;
                printf("Alias '%s' for '%s' has been created.\n", name, command);
            }
            continue;
        }

        // HISTORY FEATURE
        if (strcmp(input, "!!") == 0) {
            printf("(!! detected)");
            if (has_history == 0) {
                printf("History is empty.\n");
                continue; 
            }
            strcpy(input, history);
            printf("%s\n", input);
        } else {
            strcpy(history, input);
            has_history = 1;
        } 

        // CHECK FOR ALIASES
        apply_aliases(input);
        
        // TOKENIZATION
        int i = 0;
        char temp_input[MAX_LINE];
        strcpy(temp_input, input);
        char *token = strtok(temp_input, " \n");
        while (token != NULL){
            args[i++] = token;
            token = strtok(NULL, " \n");
        }
        args[i] = NULL;
        if (i == 0) continue;
        
        fprintf(stderr, "Executing command: %s\n", args[0]);

        // Check Pipe and RUN PIPE
        int pipe_idx = -1;
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], "|") == 0) {
                pipe_idx = j;
                break;
            }
        }

        if (pipe_idx != -1) {
            fprintf(stderr, "( | pipe detected)");
            args[pipe_idx] = NULL; 
            char **args2 = &args[pipe_idx + 1]; 
            int fd[2];
            if (pipe(fd) == -1) {
                perror("Pipe failed");
            }
        
            if (fork() == 0) {
                fprintf(stderr, "Child 1 (Left of Pipe) redirecting stdout...\n");
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]); 
                close(fd[1]);
                execvp(args[0], args);
                exit(1);
            }
        
            if (fork() == 0) {
                fprintf(stderr, "Child 2 (Right of Pipe) redirecting stdin...\n");
                dup2(fd[0], STDIN_FILENO); 
                close(fd[1]); 
                close(fd[0]);
                execvp(args2[0], args2);
                exit(1);
            }

            close(fd[0]);
            close(fd[1]);
            fprintf(stderr, "Parent waiting for piped processes to finish...\n");
            wait(NULL);
            wait(NULL);
        } else {
            // REGULAR COMMAND / REDIRECTION
            int background = 0;
            if (i > 0 && strcmp(args[i-1], "&") == 0) {
                fprintf(stderr, "(& detected) Background execution \n");
                background = 1;
                args[i-1] = NULL; 
            }
            // EXECUTE COMMAND
            execute_command(args, background);
        }
    }
    printf("Program terminated\n");
    return 0;
}