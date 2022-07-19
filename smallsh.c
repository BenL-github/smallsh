#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

static int last_status = 0;

void 
execute(char **argv){
    if(execvp(*argv, argv) < 0) perror("Exec failure"); exit(1);
}

int 
main(int argc, char* argv[]){
    struct user_input{
        // command name
        char* command;
        // array of string pointers 
        char** arguments;
        // input file name 
        char* infile;
        // output file name 
        char* outfile;
        // 1 if & was found, 0 if not 
        int background;
    };
    
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
            if(strcmp(arg, "\n") != 0){
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
            }
            arg = strtok(NULL, " ");
        }

        // printf("Total args: %d\n", total_args);
        total_command_args += total_args;
        // get the command args (arguments for the command)
        char *command_args[total_command_args + 1];
        memcpy(command_args, &arguments, (total_command_args+1)*sizeof(char*));
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
                    int outfile = open(command_args[outfile_index], O_WRONLY ,"w");
                    if(dup2(outfile, STDIN_FILENO) == -1){
                        fprintf(stderr, "Cannot open for %s output\n", command_args[outfile_index]);
                        break;
                    }
                } 
                if(infile_index > -1){
                    int infile = open(command_args[infile_index], O_RDONLY ,"r");
                    if(dup2(infile, STDOUT_FILENO) == -1){
                        fprintf(stderr, "Cannot open %s for input\n", command_args[infile_index]);
                        break;
                    }
                } 
                execute(command_args);
                last_status = 1;
                perror("Child: exec failed\n");
                exit(2); break;
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
