#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void 
execute(char** argv){
    if(execvp(*argv, argv) < 0) perror("Exec failure"); exit(1);
}

int 
main(int argc, char* argv[]){
    struct args{
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

        // initialize an array of pointers for all input arguments
        // arguments[0] = command
        // arguments[1...n] = optional arguments 
        char* arguments[512];

        // index variable to loop assign each argument an index in
        // the arguments array 
        int index = 0;

        // count total number of args
        int total_args = 0;
    
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
                if(strcmp(arg, "<")){
                    infile_index = index + 1;

                }
                // check if >
                if(strcmp(arg, ">")){ 
                    outfile_index = index + 1;
                }
                // check if &
                if(strcmp(arg, "&")){ 
                    background = 1;
                }

                arguments[index] = arg;
                index++;
                total_args++;
            }
            arg = strtok(NULL, " ");
        }
        printf("Total args: %d\n", total_args);
    
        // check if command is exit

        // check if command is cd 

        // check if command is status 

        // other commands 


    }
}
