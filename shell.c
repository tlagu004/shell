/***
 Author: Thierry Laguerre
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>  
#include <sys/stat.h>

#define MAX_LINE 80  /* The maximum length command */

int main(void)
{
    char *args[MAX_LINE/2 + 1];
    char input[MAX_LINE];
    char history[MAX_LINE]; 
    int has_history = 0;  
    int should_run = 1; 

    
    while (should_run == 1) {
        printf("osh>");
        fflush(stdout);
        
        /**
        * After reading user input, the steps are:
        * (1) fork a child process using fork()
        * (2) the child process will invoke execvp()
        * (3) parent will invoke wait() unless command included &
        */
        
        if (fgets(input, MAX_LINE, stdin) == NULL) break;
        
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            should_run = 0;
            continue;
        }
        
        if (strcmp(input, "!!") == 0) {
            if (has_history == 0) {
                printf("No commands in history.\n");
                continue; 
            }
            strcpy(input, history);
            printf("%s\n", input);
        } else {
            strcpy(history, input);
            has_history = 1;
        }

        int i = 0;
        char *token = strtok(input, " \n");
        while (token != NULL){
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        
        if (i > 0){
            int background = 0;
            if (strcmp(args[i-1], "&") == 0) {
                background = 1;
                args[i-1] = NULL; 
            }
            pid_t pid = fork();
            
            if (pid < 0) { 
                fprintf(stderr, "Fork Failed");
                return 1;
            } 
            else if (pid == 0) { 
                for (int j = 0; args[j] != NULL; j++) {
                    if (strcmp(args[j], ">") == 0) {
                        int fd = open(args[j+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            perror("Error opening file");
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO); 
                        close(fd);  
                        args[j] = NULL;
                    }

                    else if (strcmp(args[j], "<") == 0) {
                        int fd = open(args[j+1], O_RDONLY);
                        if (fd < 0) {
                            perror("Error opening file");
                            exit(1);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);                        
                        args[j] = NULL;
                    }
                }
                execvp(args[0], args);
                perror("Exec Command\n");
                return 1;
            } 
            else { 
                if (background == 0) {wait(NULL);}
                else {printf("Background process started\n");} 
            }
        }
    }

    printf("Program terminated\n");
    return 0;
}