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
static pid_t fg_process = 0;
static int fg_mode = 0;

// void 
// wait_fg(pid_t p_id){
//     // block until child process finished 
//     int childExitStatus;
//     pid_t result = waitpid(p_id, &childExitStatus, 0);

//     // check if 
//     // check child process termination
//     if(WIFSIGNALED(childExitStatus)){
//         printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
//         fflush(stdout);
//         last_status = WTERMSIG(childExitStatus);
//         terminated_signal = 1;
//     }
//     // processed finished normally 
//     else {
//         last_status = WEXITSTATUS(childExitStatus);
//         terminated_signal = 0;
//     }
//     fg_process = 0;
// }

void 
handle_SIGTSTP(){
    // wait until foreground process finishes 
    if(fg_mode){
        fg_mode = 0;
        char *msg = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, msg, 29);
        fflush(stdout);
    } 
    else {
        fg_mode = 1;
        char *msg = "Entering foreground only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, 49);
        fflush(stdout);
    }
}

void
run_exec(char **commands, char *infile, char *outfile, int background){
    // other commands 
    int childExitStatus;
    pid_t spawnPid = fork();
    switch(spawnPid){
        case -1: { perror("Hull Breach\n"); exit(1); break; }
        case 0: {
            // CHILD PROCESS

            // all child processes will ignore SIGSTP
            struct sigaction SIGSTP_action = {0};
            SIGSTP_action.sa_handler = SIG_IGN;
            sigfillset(&SIGSTP_action.sa_mask);
            SIGSTP_action.sa_flags = 0;
            sigaction(SIGSTOP, &SIGSTP_action, NULL);

            // foreground processes will terminate upon receipt of SIGINT
            if(!background){
                struct sigaction SIGINT_action = {0};
                SIGINT_action.sa_handler = SIG_DFL;
                sigfillset(&SIGINT_action.sa_mask);
                SIGINT_action.sa_flags = 0;
                sigaction(SIGINT, &SIGINT_action, NULL);
            } 

            // OUTPUT REDIRECT 
            // check:
            // 1 - there is an outfile
            // 2 - background process but no outfile 
            if(outfile || (background && !outfile)){
                int out;
                if (background) out = open("/dev/null", O_WRONLY);
                else out = open(outfile, O_WRONLY | O_CREAT);
                
                // try opening for redirect  
                if(dup2(out, STDOUT_FILENO) == -1){
                    // redirect failed - EXIT COMMAND 
                    fprintf(stderr, "Cannot open for %s output\n", outfile);
                    fflush(stderr);
                    last_status = 1;
                    break;
                }
            } 

            // INPUT REDIRECTION
            // check if:
            // 1 - there is an infile
            // 2 - background process but no infile 
            if(infile || (background && !infile)){
                int in;
                if (background) in = open("/dev/null", O_RDONLY);
                else in = open(infile, O_RDONLY);
                
                // try opening for redirect
                if(dup2(in, STDIN_FILENO) == -1){
                    // redirect failed - EXIT COMMAND
                    fprintf(stderr, "Cannot open %s for input\n", infile);
                    fflush(stderr);
                    last_status = 1;
                    break;
                }
            }

            // execute command 
            if(execvp(*commands, commands) < 0){
                // command not found or failed 
                last_status = 1;
                fprintf(stderr, "%s: no such file or directory\n", commands[0]);
                fflush(stderr);
            }
            break;
        }
        default: {
            // PARENT PROCESS 

            // FOREGROUND PROCESS 
            if(!background){
                pid_t result = waitpid(spawnPid, &childExitStatus, 0);
                // check child process termination
                if(WIFSIGNALED(childExitStatus)){
                    printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
                    fflush(stdout);
                    last_status = WTERMSIG(childExitStatus);
                    terminated_signal = 1;
                }
                // processed finished normally 
                else {
                    last_status = WEXITSTATUS(childExitStatus);
                    terminated_signal = 0;
                }
                break;
            }

            // BACKGROUND PROCESS 
            // do not block 
            waitpid(spawnPid, &childExitStatus, WNOHANG);
            printf("background pid is %d\n", spawnPid);
            fflush(stdout);
            break;
        }
    }
}

void 
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
            printf("terminated by signal %d\n", last_status);
        }
        else printf("exit value %d\n", last_status);
        fflush(stdout);
    }
    // run exec()
    else {
        run_exec(commands, infile, outfile, background);
    }
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
        fflush(stdout);

        // print the prompt
        printf(": ");
        fflush(stdout);

        // create buffer for input and get user input as a line
        char input[2048] = "";
        fgets(input, 2048, stdin);
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
            } else if((strcmp(arg, "&") == 0) && !fg_mode){ 
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

        // run the command 
        run_cmd(command_args, infile, outfile, background);
    }
}

int 
main(int argc, char* argv[]){
    // signal hander to ignore SIGINT (ctrl + c)
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // signal handler for SIGSTP
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // run the shell
    shell();
}
