#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#define MAX_ARGS 10
#define MAX_INPUT_SIZE 1024

void handle_cat(char **args);  // Declare the function

int main() {
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARGS];
    int i = 0;

    // Prompt user for input
    printf("quash> ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        perror("Failed to read input");
        return 1;
    }

    // Remove newline character from input if present
    input[strcspn(input, "\n")] = 0;

    // Tokenize the input and store in args array
    char *token = strtok(input, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // Null-terminate args array

    // Debug print to confirm args are parsed correctly
    printf("Debug: Parsed arguments:\n");
    for (int j = 0; args[j] != NULL; j++) {
        printf("args[%d] = %s\n", j, args[j]);
    }

    // Call handle_cat with parsed arguments
    handle_cat(args);

    return 0;
}

// Define the handle_cat function
void handle_cat(char **args) {
    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;

    // First, parse args for redirection operators and set up input/output
    for (int i = 1; args[i] != NULL; i++) {
        printf("Debug: Current argument - %s\n", args[i]);
        if (strcmp(args[i], "<") == 0) {  // Input redirection
            in_fd = open(args[i + 1], O_RDONLY);
            if (in_fd == -1) {
                perror("Failed to open input file");
                return;
            }
            printf("Debug: Input redirection set to file %s\n", args[i + 1]);
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--;  // Re-check the shifted argument
        } else if (strcmp(args[i], ">") == 0) {  // Output redirection (overwrite)
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("Failed to open output file for overwriting");
                return;
            }
            printf("Debug: Output redirection (overwrite) set to file %s\n", args[i + 1]);
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--;
        } else if (strcmp(args[i], ">>") == 0) {  // Output redirection (append)
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (out_fd == -1) {
                perror("Failed to open output file for appending");
                return;
            }
            printf("Debug: Output redirection (append) set to file %s\n", args[i + 1]);
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--;
        }
    }

    // Fork to handle reading and writing separately
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return;
    } else if (pid == 0) {  // Child process for reading and writing
        // Redirect stdin if needed
        if (in_fd != STDIN_FILENO) {
            if (dup2(in_fd, STDIN_FILENO) == -1) {
                perror("Failed to redirect stdin");
                exit(1);
            }
            close(in_fd);
        }

        // Redirect stdout if needed
        if (out_fd != STDOUT_FILENO) {
            if (dup2(out_fd, STDOUT_FILENO) == -1) {
                perror("Failed to redirect stdout");
                exit(1);
            }
            close(out_fd);
        }

        // Process files if specified, or read from stdin if none
        if (args[1] == NULL) {
            printf("Debug: No files specified, reading from stdin\n");
            char buffer[1024];
            ssize_t bytes_read;
            while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
                if (write(STDOUT_FILENO, buffer, bytes_read) == -1) {
                    perror("Failed to write to output");
                    break;
                }
            }
        } else {
            // Loop through each file and read contents
            for (int i = 1; args[i] != NULL; i++) {
                printf("Debug: Processing file %s\n", args[i]);
                int fd = open(args[i], O_RDONLY);
                if (fd == -1) {
                    perror("Failed to open input file");
                    continue;
                }

                // Read file and write contents
                char buffer[1024];
                ssize_t bytes_read;
                while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                    if (write(STDOUT_FILENO, buffer, bytes_read) == -1) {
                        perror("Failed to write to output");
                        close(fd);
                        return;
                    }
                }
                close(fd);  // Close each file after reading
            }
        }
        _exit(0);  // Exit child process
    } else {  // Parent process
        // Close unused output descriptor in the parent
        if (out_fd != STDOUT_FILENO) {
            close(out_fd);
        }
        wait(NULL);  // Wait for child to finish
    }
}