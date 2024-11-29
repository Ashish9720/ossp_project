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
    if (buf[0] == ' ' || buf[0] == '\n')
        memmove(buf, buf + 1, strlen(buf));
}

/*
tokenizes char* buf using the delimiter c, and returns the array of strings in param
and the size of the array in pointer nr
*/
void tokenize_buffer(char** param, int *nr, char *buf, const char *c) {
    char *token;
    token = strtok(buf, c);
    int pc = -1;
    while (token) {
        param[++pc] = malloc(strlen(token) + 1);
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
void print_params(char **param) {
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

    if (fork() > 0) {
        // Parent
        wait(NULL);
    } else {
        // Child
        execvp(argv[0], argv);
        // In case exec is not successful, exit
        perror(ANSI_COLOR_RED "Invalid input" ANSI_COLOR_RESET "\n");
        exit(1);
    }
}

/*
loads and executes a series of external commands that are piped together
*/
void executePiped(char** buf, int nr) { // Can support up to 10 piped commands
    if (nr > 10) return;

    int fd[10][2], i, pc;
    char *argv[100];

    for (i = 0; i < nr; i++) {
        tokenize_buffer(argv, &pc, buf[i], " ");
        // Replace "sa" with "ls" in piped commands
        if (strcmp(argv[0], "sa") == 0) {
            argv[0] = "ls";
        }

        if (i != nr - 1) {
            if (pipe(fd[i]) < 0) {
                perror("Pipe creation failed\n");
                return;
            }
        }
        if (fork() == 0) { // Child
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
            perror("Invalid input");
            exit(1);
        }
        // Parent
        if (i != 0) { // Close previous pipe
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL);
    }
}

/*
loads and executes a series of external commands that have to be run asynchronously
*/
void executeAsync(char** buf, int nr) {
    int i, pc;
    char *argv[100];
    for (i = 0; i < nr; i++) {
        tokenize_buffer(argv, &pc, buf[i], " ");
        if (fork() == 0) {
            execvp(argv[0], argv);
            perror("Invalid input");
            exit(1);
        }
    }
    for (i = 0; i < nr; i++) {
        wait(NULL);
    }
}

/*
loads and executes an external command that needs file redirection(input, output or append)
*/
void executeRedirect(char** buf, int nr, int mode) {
    int pc, fd;
    char *argv[100];
    removeWhiteSpace(buf[1]);
    tokenize_buffer(argv, &pc, buf[0], " ");
    if (fork() == 0) {

        switch (mode) {
        case INPUT:  fd = open(buf[1], O_RDONLY); break;
        case OUTPUT: fd = open(buf[1], O_WRONLY | O_CREAT | O_TRUNC, 0644); break;
        case APPEND: fd = open(buf[1], O_WRONLY | O_CREAT | O_APPEND, 0644); break;
        default: return;
        }

        if (fd < 0) {
            perror("Cannot open file\n");
            return;
        }

        switch (mode) {
        case INPUT:   dup2(fd, 0); break;
        case OUTPUT:  dup2(fd, 1); break;
        case APPEND:  dup2(fd, 1); break;
        default: return;
        }
        execvp(argv[0], argv);
        perror("Invalid input");
        exit(1); // In case exec is not successful, exit
    }
    wait(NULL);
}

/*
shows the internal help
*/
void showHelp() {
    printf(ANSI_COLOR_GREEN "----------Help--------" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Supported commands: sa (ls), echo, cd, pwd, exit" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Piping: Ex. sa | echo \"This works\"" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Asynchronous: Ex. sa & echo \"Done!\"" ANSI_COLOR_RESET "\n");
    printf(ANSI_COLOR_GREEN "Input/output redirection: Ex. sa > file, sa >> file, echo < file" ANSI_COLOR_RESET "\n");
}

int main() {
    char buf[500], *buffer[100], cwd[1024];
    int nr = 0;
    printf(ANSI_COLOR_GREEN "**************************CUSTOM SHELL***************************" ANSI_COLOR_RESET "\n");

    while (1) {
        printf(ANSI_COLOR_BLUE "Enter command (or 'exit' to quit):" ANSI_COLOR_RESET "\n");
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf(ANSI_COLOR_GREEN "%s $ " ANSI_COLOR_RESET, cwd);
        else
            perror("getcwd failed\n");

        fgets(buf, 500, stdin);
        buf[strcspn(buf, "\n")] = 0; // Remove newline

        if (strchr(buf, '|')) {
            tokenize_buffer(buffer, &nr, buf, "|");
            executePiped(buffer, nr);
        } else if (strchr(buf, '&')) {
            tokenize_buffer(buffer, &nr, buf, "&");
            executeAsync(buffer, nr);
        } else if (strstr(buf, ">>")) {
            tokenize_buffer(buffer, &nr, buf, ">>");
            if (nr == 2) executeRedirect(buffer, nr, APPEND);
            else printf("Incorrect redirection format\n");
        } else if (strchr(buf, '>')) {
            tokenize_buffer(buffer, &nr, buf, ">");
            if (nr == 2) executeRedirect(buffer, nr, OUTPUT);
            else printf("Incorrect redirection format\n");
        } else if (strchr(buf, '<')) {
            tokenize_buffer(buffer, &nr, buf, "<");
            if (nr == 2) executeRedirect(buffer, nr, INPUT);
            else printf("Incorrect redirection format\n");
        } else {
            char *params[100];
            tokenize_buffer(params, &nr, buf, " ");
            if (strcmp(params[0], "cd") == 0) {
                if (chdir(params[1]) < 0) perror("cd failed");
            } else if (strcmp(params[0], "help") == 0) {
                showHelp();
            } else if (strcmp(params[0], "exit") == 0) {
                printf(ANSI_COLOR_GREEN "Goodbye!" ANSI_COLOR_RESET "\n");
                exit(0);
            } else {
                executeBasic(params);
            }
        }
    }
}
