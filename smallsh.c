#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

static int last_status = 0;

int 
main(int argc, char* argv[]){
    while(1){
        printf(": ");
        fflush(stdout);

        // create buffer for input and get user input as a line
        char *input = NULL;
        size_t len = 2048;
        getline(&input, &len, stdin);
        // remove the newline character 
        input[strlen(input)-1] = '\0';

        // initialize an array of pointers for all input arguments
        // arguments[0] = command
        // arguments[1...n] = optional arguments 
        char* arguments[512];

        // index variable to loop assign each argument an index in
        // the arguments array 
        int index = 0;

        // count total number of args
        int total_args = 0;
        int total_command_args = 0;
    
        // variables to mark indices of infile & outfiles
        // -1 means user did not input an in/outfile
        // any other nonnegative number indicate the indices of the in/outfile
        int infile_index = -1;
        int outfile_index = -1;

        // 0 means the command does not contain '&', 1 otherwise 
        int background = 0;

        // use strtok to parse arguments 
        char *arg = strtok(input, " ");

        // check if first arg is # 
        if(strcmp(arg, "#") == 0) continue;

        while(arg){
            
            // check if <
            if(strcmp(arg, "<") == 0){ 
                infile_index = index + 1; 
                total_command_args -= 2;
            }
            // check if >
            if(strcmp(arg, ">") == 0){ 
                outfile_index = index + 1; 
                total_command_args -= 2;
            }
            // check if &
            if(strcmp(arg, "&") == 0){ 
                background = 1; 
                total_command_args--;
            }
            arguments[index] = arg;
            index++;
            total_args++;
            
            arg = strtok(NULL, " ");
        }

        // printf("Total args: %d\n", total_args);
        
        // get the arguments for the command, not including
        // the redirect operators, redirect files, and '&'
        // >>> these arguments are for the exec() funciton <<<
        total_command_args += total_args;
        char *command_args[total_command_args + 1];
        memcpy(command_args, &arguments, (total_command_args+1)*sizeof(char*));
        // exec() requires the last value to be a null pointer 
        command_args[total_command_args] = NULL;

        // for(int i = 0; i < total_command_args; i++){
        //     printf("%s\n", command_args[i]);
        // }

        // check if command is exit
        if(strcmp(command_args[0], "exit") == 0){
            // send signal to stop child processes
            kill(0, SIGKILL);
            if((last_status = wait(0)) < 0) err(errno, "Wait failed");
            exit(EXIT_SUCCESS);
        }

        // check if command is cd 
        if(strcmp(command_args[0], "cd") == 0){ 
            if(total_args == 1) chdir(getenv("HOME"));
            else chdir(command_args[1]);
            continue;
        }
        // check if command is status 
        if(strcmp(command_args[0], "status") == 0){ 
            printf("exit value %d\n", last_status);
            continue;
        }

        // other commands 
        pid_t spawnPid = -5;
        int childExitStatus = -5;

        spawnPid = fork();
        switch(spawnPid){
            case -1: { perror("Hull Breach\n"); exit(1); break; }
            case 0: {
                if(outfile_index > -1){
                    int outfile = open(arguments[outfile_index], O_WRONLY | O_CREAT,"w");
                    if(dup2(outfile, STDOUT_FILENO) == -1){
                        fprintf(stderr, "Cannot open for %s output\n", arguments[outfile_index]);
                        last_status = 1;
                        close(outfile);
                        break;
                    }
                } 
                if(infile_index > -1){
                    int infile = open(arguments[infile_index], O_RDONLY ,"r");
                    if(dup2(infile, STDIN_FILENO) == -1){
                        fprintf(stderr, "Cannot open %s for input\n", arguments[infile_index]);
                        last_status = 1;
                        close(infile);
                        break;
                    }
                } 
                if(execvp(*command_args, command_args) < 0){
                    last_status = 1;
                    perror("Child: exec failed\n");
                }
                break;
            }
            default: {
                pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
                if(strcmp(command_args[total_command_args - 1], "&") != 0){
                    last_status = WEXITSTATUS(childExitStatus);
                }
                break;
            }
        }
    }
}
