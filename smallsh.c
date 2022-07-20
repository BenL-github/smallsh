#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

static int last_status = 0;
static int terminated_signal = 0;

int 
run_exec(char **commands, char *infile, char *outfile, int background){
    // other commands 
    int childExitStatus;
    pid_t spawnPid = fork();
    switch(spawnPid){
        case -1: { perror("Hull Breach\n"); exit(1); break; }
        case 0: {
            // child process will run
            // perform output redirect 
            if(outfile || (background && !outfile)){
                int out;
                if (background) {
                    out = open("/dev/null", O_WRONLY);
                }
                else {
                    out = open(outfile, O_WRONLY | O_CREAT);
                }

                if(dup2(out, STDOUT_FILENO) == -1){
                    fprintf(stderr, "Cannot open for %s output\n", outfile);
                    last_status = 1;
                    close(out);
                    break;
                }
            } 

            // perform input redirect
            if(infile || (background && !infile)){
                int in;
                if (background) in = open("/dev/null", O_RDONLY);
                else in = open(infile, O_RDONLY);

                if(dup2(in, STDIN_FILENO) == -1){
                    fprintf(stderr, "Cannot open %s for input\n", infile);
                    last_status = 1;
                    close(in);
                    break;
                }
            }

            // foreground processes will terminate upon
            // recipe of SIGINT
            if(!background){
                struct sigaction shell_action = {0};
                shell_action.sa_handler = SIG_DFL;
                sigfillset(&shell_action.sa_mask);
                shell_action.sa_flags = 0;
                sigaction(SIGINT, &shell_action, NULL);
            }

            if(execvp(*commands, commands) < 0){
                last_status = 1;
                fprintf(stderr, "%s: no such file or directory\n", commands[0]);
            }

            break;
        }
        default: {
            // parent process
            if(!background){
                // foreground process
                pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
                if(WIFSIGNALED(childExitStatus)){
                    printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
                    last_status = WTERMSIG(childExitStatus);
                }
                else last_status = WEXITSTATUS(childExitStatus);
                
                break;
            }
            // if background process: run in background 
            waitpid(spawnPid, &childExitStatus, WNOHANG);
            printf("background pid is %d\n", spawnPid);
            fflush(stdout);
            break;
        }
    }

    return 1;
}
int 
run_cmd(char **commands, char *infile, char *outfile, int background){
    // BUILT-IN COMMANDS
    if(strcmp(commands[0], "exit") == 0){
        // send signal to stop child processes
        kill(0, SIGKILL);
        if((last_status = wait(0)) < 0) err(errno, "Wait failed");
        exit(EXIT_SUCCESS);
    }
    // check if command is cd 
    else if(strcmp(commands[0], "cd") == 0){ 
        if(sizeof(commands)/sizeof(char*) == 1) chdir(getenv("HOME"));
        else chdir(commands[1]);
    }
    // check if command is status 
    else if(strcmp(commands[0], "status") == 0){ 
        if(terminated_signal){
            printf("terminated by signal %d", last_status);
        }
        printf("exit value %d\n", last_status);
    }
    else{
        run_exec(commands, infile, outfile, background);
    }
    return 1;
}

void 
shell(){
    while(1){
        // print all background processes that have finished 
        int bg_status;
        pid_t finished_bg_process;
        while((finished_bg_process = waitpid(-1, &bg_status, WNOHANG)) > 0){
            // check if signal was terminated 
            if(WIFSIGNALED(bg_status)){
                printf("background pid %d is done: terminated by signal %d", 
                    finished_bg_process, WTERMSIG(bg_status));
            } else {
                // exited normally
                printf("background pid %d is done: exit value %d\n", 
                    finished_bg_process, bg_status);
            }
        }

        // print the prompt
        printf(": ");
        fflush(stdout);

        // create buffer for input and get user input as a line
        char *input = NULL;
        size_t len = 2048; // len is just a buffer => not the max length => >>>>>>>> FIX <<<<<<<<<<<
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

        // count total number of command arguments (NOT redirection/file/background args)
        int total_command_args = 0;
    
        // variables to mark indices of infile & outfiles
        // -1 means user did not input an in/outfile
        // any other nonnegative number indicate the indices of the in/outfile
        int infile_index = -1;
        int outfile_index = -1;
        char *outfile = NULL;
        char *infile = NULL;

        // 0 means the command does not contain '&', 1 otherwise 
        int background = 0;

        // parse arguments 
        char *arg = strtok(input, " ");
        // check if first arg is # 
        if(!arg || strcmp(arg, "#") == 0) continue;

        while(arg){
            // check if <, >, or &
            if(strcmp(arg, "<") == 0){ 
                infile_index = index + 1;
                // subtract 2 args for the redirect + filename
                total_command_args -= 1;
            } else if(strcmp(arg, ">") == 0){ 
                outfile_index = index + 1; 
                // subtract 2 args for the redirect + filename
                total_command_args -= 1;
            } else if(strcmp(arg, "&") == 0){ 
                background = 1; 
            } else {
                total_command_args++;
            }
            // place arg into array and increment index
            arguments[index] = arg;
            index++;

            // TODO: EXPANSION
            if(strcmp(arg, "$$") == 0){
            }
            
            arg = strtok(NULL, " ");
        }

        // get outfile and infile names if exists 
        if (outfile_index >= 0) outfile = arguments[outfile_index];
        if (infile_index >= 0) infile = arguments[infile_index];

        // get the arguments for the command, not including
        // the redirect operators, redirect files, and '&'
        // >>> these arguments are for the exec() funciton <<<
        char *command_args[total_command_args + 1];
        memcpy(command_args, &arguments, (total_command_args+1)*sizeof(char*));
        command_args[total_command_args] = NULL;

        // built in 
        run_cmd(command_args, infile, outfile, background);
    }
}

int 
main(int argc, char* argv[]){
    // signal hander to have smallsh ignore SIGINT (ctrl + c)
    struct sigaction shell_action = {0};
    shell_action.sa_handler = SIG_IGN;
    sigfillset(&shell_action.sa_mask);
    shell_action.sa_flags = 0;
    sigaction(SIGINT, &shell_action, NULL);

    // run the shell
    shell();
}
