#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>

// track the last exit status
static int last_status = 0;
// track the terminating signal
static int terminated_signal = 0;
// set to 0 if the shell is not in foreground mode
// set to 1 if the shell is in foreground mode
volatile sig_atomic_t fg_mode = -1;

/*
 * Function: handle_SIGTSTP
 *
 * Signal handler for SIGTSTP. 
 * Changes whether the shell will enter foreground mode
 * or exit foreground mode. Prints a message. 
 *
 * returns: void 
*/
void 
handle_SIGTSTP(){
    if(fg_mode == 1){
        // Exit foreground mode 
        fg_mode = 0;
        char *msg = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, msg, 29);
        fflush(stdout);
    } 
    else {
        // Enter foreground mode 
        fg_mode = 1;
        char *msg = "Entering foreground only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, 49);
        fflush(stdout);
    }
}

/*
 * Function: run_exec
 *
 * Run non-builtin commands with exec()
 *
 * commands: list of command and its arguments
 * infile: the input file name
 * outfile: the output file name 
 * background: 0 to run in the foreground, 1 to run in the background 
 *
 * returns: void 
*/
void
run_exec(char **commands, char *infile, char *outfile, int background){
    // Fork a process 
    int childExitStatus = -5;
    pid_t spawnPid = fork();
    switch(spawnPid){
        case -1: { perror("Hull Breach\n"); exit(1); break; }
        case 0: {
            // CHILD PROCESS

            // all child processes will ignore SIGTSTP
            struct sigaction SIGTSTP_action = {0};
            SIGTSTP_action.sa_handler = SIG_IGN;
            sigfillset(&SIGTSTP_action.sa_mask);
            SIGTSTP_action.sa_flags = 0;
            sigaction(SIGTSTP, &SIGTSTP_action, NULL);

            // signal handler for SIGUSR1
            struct sigaction SIGUSR1_action = {0};
            SIGUSR1_action.sa_handler = SIG_DFL;
            sigfillset(&SIGUSR1_action.sa_mask);
            SIGUSR1_action.sa_flags = 0;
            sigaction(SIGUSR1, &SIGUSR1_action, NULL);

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
                else out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                
                // try opening for redirect  
                if(dup2(out, STDOUT_FILENO) == -1){
                    // redirect failed - EXIT COMMAND 
                    fprintf(stderr, "Cannot open %s for output\n", outfile);
                    last_status = 1;
                    exit(2);
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
                    last_status = 1;
                    exit(2);
                    break;
                }
            }

            // execute command 
            if(execvp(*commands, commands) < 0){
                // command not found or failed 
                last_status = 1;
                fprintf(stderr, "%s: no such file or directory\n", commands[0]);
                fflush(stderr);
                exit(2);
            }
            break;
        }
        default: {
            // PARENT PROCESS 

            // FOREGROUND PROCESS 
            if(!background){
                pid_t result;
                // block SIGTSTP
                sigset_t new_mask;
                sigemptyset(&new_mask);
                sigaddset(&new_mask, SIGTSTP);
                sigprocmask(SIG_BLOCK, &new_mask, NULL);

                result = waitpid(spawnPid, &childExitStatus, 0);
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
                // unblock SIGTSTP
                sigprocmask(SIG_UNBLOCK, &new_mask, NULL);
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

/*
 * Function: run_cmd
 *
 * Runs a command based on user input 
 *
 * commands: list of command and its arguments
 * infile: the input file name
 * outfile: the output file name 
 * background: 0 to run in the foreground, 1 to run in the background 
 *
 * returns: void 
*/
void 
run_cmd(char **commands, char *infile, char *outfile, int background){
    // BUILT-IN COMMANDS
    if(strcmp(commands[0], "exit") == 0){
        // send signal to stop child processes
        kill(0, SIGUSR1);
        // wait until all child process terminated
        wait(0);
        free(commands[0]);
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

/*
 * Function: expand
 *
 * Expands any '$$' in a string to the shell's process id
 * Example: 'words$$words' will return 'words[processid]words'
 *
 * arg should be the address of the first element of a string 
 *
 * returns: pointer to the new string 
*/

char* 
expand(char *arg){
    // create a buffer to store the shell's PID as a string
    char pid_str[15];
    sprintf(pid_str, "%jd", (intmax_t) getpid());
    size_t pid_length = strlen(pid_str);

    // Calculate extra memory needed for expansion
    size_t extra_mem_needed = 0;
    char *ptr1 =  arg;
    char *ptr2 = &arg[1];
    while(*ptr2){
        // check if *ptr1 and *ptr2 are '&'
        if(*ptr1 == '$' && *ptr2 == '$'){
            extra_mem_needed += pid_length;
            ptr1 += 2;
            ptr2 += 2;
        }
        else{
            ptr1 += 1;
            ptr2 += 1;
        }
    }

    // Build the new string 
    // The new string length is:
    // length of arg + extra mem for the pid - # of '$$' to be replaced + null term
    char *new = malloc(strlen(arg)+extra_mem_needed-(2*extra_mem_needed/pid_length)+1);
    ptr1 =  arg;
    ptr2 = &arg[1];
    char *index = new;

    // loop through the argument and add the letters to the new string
    while(*ptr2){
        // null terminate to current point in the new string
        *index = '\0';
        // check if *ptr1 and *ptr2 are '&'
        if(*ptr1 == '$' && *ptr2 == '$')
        {
            // cat the pid to new string
            strcat(new, pid_str);
            // update ptr locations
            index += pid_length;
            ptr1 += 2;
            ptr2 += 2;
        }
        else {
            // add letter to the new string 
            *index = *ptr1;
            // update ptr locations
            index ++;
            ptr1 ++;
            ptr2 ++;
        }
    }
    // add last element to the string
    if(*ptr1){
        *index = *ptr1;
    }
    // null terminate string 
    new[strlen(arg)+extra_mem_needed-(2*extra_mem_needed/pid_length)] = '\0';
    // return expanded string 
    return new;
}

/*
 * Function: shell
 *
 * Runs the shell. Includes a subset of functions used in 
 * other popular shells such as bash. 
 *
 * returns: void 
*/
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
        if(!arg || arg[0] == '#') continue;

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
                // do nothing 
            } else {
                total_command_args++;
            }

            // expand arg and place into arg array, increment index
            char *expanded = expand(arg);
            arguments[index] = expanded;
            index++;
            
            arg = strtok(NULL, " ");
        }

        // get outfile and infile names if exists 
        if (outfile_index >= 0) outfile = arguments[outfile_index];
        if (infile_index >= 0) infile = arguments[infile_index];
        // check if last index is '&'

        if ((strcmp(arguments[index - 1], "&") == 0) && fg_mode != 1) {
            background = 1;
        }

        // get the arguments for the command, not including
        // the redirect operators, redirect files, and '&'
        // >>> these arguments are for the exec() funciton <<<
        char *command_args[total_command_args + 1];
        memcpy(command_args, &arguments, (total_command_args+1)*sizeof(char*));
        command_args[total_command_args] = NULL;

        // run the command 
        run_cmd(command_args, infile, outfile, background);

        // free the allocated memory to store the commands
        for(int i = 0; i < index; i++){
            free(arguments[i]);
        }
    }
}

int 
main(int argc, char* argv[]){
    // char *my_string = "$";
    // char *expanded = expand(my_string);

    // printf("Expanded: %s\n", expanded);

    // signal hander to ignore SIGINT (ctrl + c)
    // This signal terminated foreground processes
    // but not the shell itself 
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // signal handler for SIGTSTP
    // This signal is used to run shell in foreground-only mode
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // signal handler for SIGUSR1
    // This signal is used to terminate child processes 
    struct sigaction SIGUSR1_action = {0};
    SIGUSR1_action.sa_handler = SIG_IGN;
    sigfillset(&SIGUSR1_action.sa_mask);
    SIGUSR1_action.sa_flags = 0;
    sigaction(SIGUSR1, &SIGUSR1_action, NULL);

    // run the shell
    shell();
}
