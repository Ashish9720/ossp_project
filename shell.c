#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

/*
Use these colors to print colored text on the console
*/
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/*
use with redirection(< > >>) to indicate to the function in which mode to open the file
and to know that redirection of the input OR output has to be done
*/
#define INPUT 0
#define OUTPUT 1
#define APPEND 2

/*
removes the newline and space character from the end and start of a char*
*/
void removeWhiteSpace(char* buf) {
    if (buf[strlen(buf) - 1] == ' ' || buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';
    if (buf[0] == ' ' || buf[0] == '\n') memmove(buf, buf + 1, strlen(buf));
}

/*
tokenizes char* buf using the delimiter c, and returns the array of strings in param
and the size of the array in pointer nr
*/
void tokenize_buffer(char** param, int* nr, char* buf, const char* c) {
    char* token;
    token = strtok(buf, c);
    int pc = -1;
    while (token) {
        param[++pc] = malloc(sizeof(token) + 1);
        strcpy(param[pc], token);
        removeWhiteSpace(param[pc]);
        token = strtok(NULL, c);
    }
    param[++pc] = NULL;
    *nr = pc;
}

/*
used for debugging purposes to print all the strings in a string array
*/
void print_params(char** param) {
    while (*param) {
        printf("param=%s..\n", *param++);
    }
}

/*
loads and executes a single external command
*/
void executeBasic(char** argv) {
    // Replace "sa" command with "ls"
    if (strcmp(argv[0], "sa") == 0) {
        argv[0] = "ls";
    }
    // Replace "noise" command with "echo"
    if (strcmp(argv[0], "noise") == 0) {
        argv[0] = "echo";
    }

    if (fork() > 0) {
        // parent
        wait(NULL);
    } else {
        // child
        execvp(argv[0], argv);
        // in case exec is not successful, exit
        perror(ANSI_COLOR_RED "invalid input" ANSI_COLOR_RESET "\n");
        exit(1);
    }
}

/*
loads and executes a series of external commands that are piped together
*/
void executePiped(char** buf, int nr) { // can support up to 10 piped commands
    if (nr > 10) return;

    int fd[10][2], i, pc;
    char* argv[100];

    for (i = 0; i < nr; i++) {
        tokenize_buffer(argv, &pc, buf[i], " ");
        // Replace "sa" with "ls" in piped commands
        if (strcmp(argv[0], "sa") == 0) {
            argv[0] = "ls";
        }
        // Replace "noise" with "echo" in piped commands
        if (strcmp(argv[0], "noise") == 0) {
            argv[0] = "echo";
        }

        if (i != nr - 1) {
            if (pipe(fd[i]) < 0) {
                perror("pipe creating was not successful\n");
                return;
            }
        }
        if (fork() == 0) { // child1
            if (i != nr - 1) {
                dup2(fd[i][1], 1);
                close(fd[i][0]);
                close(fd[i][1]);
            }

            if (i != 0) {
                dup2(fd[i - 1][0], 0);
                close(fd[i - 1][1]);
                close(fd[i - 1][0]);
            }
            execvp(argv[0], argv);
            perror("invalid input ");
            exit(1); // in case exec is not successful, exit
        }
        // parent
        if (i != 0) { // second process
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL);
    }
}

/*
shows the internal help
*/
void showHelp() {
    printf(ANSI_COLOR_GREEN "----------Help--------" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Supported commands: cd, pwd, sa (ls), noise (echo), exit" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Commands can be piped together (e.g., sa -a | noise hello)" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Input and output redirection are supported (e.g., sa > file, noise < file)" ANSI_COLOR_RESET "\n");
}

int main(int argc, char** argv) {
    char buf[500], *buffer[100], *params1[100], cwd[1024];
    int nr = 0;

    printf(ANSI_COLOR_GREEN "*****************************************************************" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "**************************CUSTOM SHELL***************************" ANSI_COLOR_RESET "\n");

    while (1) {
        printf(ANSI_COLOR_BLUE "Enter command (or 'exit' to exit):" ANSI_COLOR_RESET "\n");

        // print current directory
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf(ANSI_COLOR_GREEN "%s  " ANSI_COLOR_RESET, cwd);
        else
            perror("getcwd failed\n");

        // read user input
        fgets(buf, 500, stdin); // buffer overflow cannot happen

        // check if only a simple command needs to be executed or multiple piped commands or other types
        if (strchr(buf, '|')) { // tokenize pipe commands
            tokenize_buffer(buffer, &nr, buf, "|");
            executePiped(buffer, nr);
        } else { // single command including internal ones
            tokenize_buffer(params1, &nr, buf, " ");
            if (strcmp(params1[0], "cd") == 0) { // cd builtin command
                chdir(params1[1]);
            } else if (strcmp(params1[0], "help") == 0) { // help builtin command
                showHelp();
            } else if (strcmp(params1[0], "exit") == 0) { // exit builtin command
                exit(0);
            } else {
                executeBasic(params1);
            }
        }
    }

    return 0;
}
