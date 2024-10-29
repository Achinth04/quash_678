#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_COUNT 100
#define MAX_JOBS 100

// Job structure
typedef struct {
    int job_id;
    pid_t pid;
    char command[MAX_INPUT_SIZE];
    int active;
} Job;

// Global job list
Job jobs[MAX_JOBS];
int job_count = 0;

// Function prototypes
void execute_command(char *input);
int handle_builtin_commands(char **args);
void execute_external_command(char **args, int background);
void check_background_jobs();
void add_job(pid_t pid, char *command);
void print_jobs();
void remove_job(pid_t pid);
void kill_process(char **args);
void kill_job_by_id(int job_id);
void export_variable(char *arg);
void handle_grep(char **arg);
void handle_find(char **args) ;
// Built-in command function prototypes
void quash_pwd();
void quash_echo(char **args);
void quash_cd(char **args);

// Helper function to tokenize input
char **tokenize_input(char *input);

// Main function to handle Quash shell loop
int main() {
    char input[MAX_INPUT_SIZE];

    while (1) {
        printf("quash> ");
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            perror("Error reading input");
            continue;
        }

        // Remove newline from input
        input[strcspn(input, "\n")] = 0;

        // Check for and manage completed background jobs
        check_background_jobs();

        // Tokenize and execute the command
        execute_command(input);
    }

    return 0;
}

// Function to tokenize the input
char **tokenize_input(char *input) {
    char **tokens = malloc(MAX_ARG_COUNT * sizeof(char *));
    char *token;
    int position = 0;

    token = strtok(input, " ");
    while (token != NULL) {
        tokens[position++] = token;
        token = strtok(NULL, " ");
    }
    tokens[position] = NULL; // Null-terminate the array

    return tokens;
}

// Function to determine if the command is built-in or external
void execute_command(char *input) {
    int background = 0;

    // Check if command ends with '&'
    if (input[strlen(input) - 1] == '&') {
        background = 1;
        input[strlen(input) - 1] = '\0';  // Remove the '&' from input
    }

    // Tokenize the input
    char **args = tokenize_input(input);

    // If empty input, return early
    if (args[0] == NULL) {
        free(args);
        return;
    }

    // Check if the command is a built-in command
    if (!handle_builtin_commands(args)) {
        // If not built-in, try to execute it as an external command
        execute_external_command(args, background);
    }

    free(args);
}

// Function to handle built-in commands (returns 1 if command is built-in, 0 otherwise)
int handle_builtin_commands(char **args) {
    if (strcmp(args[0], "pwd") == 0) {
        quash_pwd();
        return 1;
    } else if (strcmp(args[0], "echo") == 0) {
        quash_echo(args);
        return 1;
    } else if (strcmp(args[0], "cd") == 0) {
        quash_cd(args);
        return 1;
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0); // Direct exit from shell
    } else if (strcmp(args[0], "jobs") == 0) {
        print_jobs();
        return 1;
    } else if (strcmp(args[0], "export") == 0) {
        if (args[1] != NULL) {
            export_variable(args[1]);
        } else {
            printf("Usage: export VAR=VALUE\n");
        }
        return 1;
    
    } else if (strcmp(args[0], "grep") == 0) {
        handle_grep(args);  // Call handle_grep for 'grep' command
        return 1;
    }

    return 0; // Not a built-in command
}


// Built-in function to handle 'pwd' command
void quash_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd() error");
    }
}


// Helper function to check for environment variables
char* handle_env_variables(char* arg) {
    if (arg[0] == '$') {
        // Skip the '$' and try to get the environment variable value
        char* value = getenv(arg + 1);
        if (value) {
            return value;  // Return the environment variable's value
        } else {
            return "";  // If the environment variable is not found, return an empty string
        }
    }
    return arg;  // If it's not an environment variable, return the original argument
}

// Helper function to remove surrounding quotes (both ' and ")
char* remove_quotes(char* arg) {
    size_t len = strlen(arg);
    if ((arg[0] == '\'' && arg[len - 1] == '\'') || (arg[0] == '\"' && arg[len - 1] == '\"')) {
        // Remove the first and last character (quotes)
        arg[len - 1] = '\0';  // Remove the closing quote
        return arg + 1;  // Skip the opening quote
    }
    return arg;  // Return the argument unchanged if no quotes are found
}

// Built-in function to handle 'echo' command with $ENV handling and quote removal
void quash_echo(char **args) {
    int out_fd = STDOUT_FILENO;
    int arg_idx = 1;

    // Check for output redirection (">" or ">>")
    while (args[arg_idx] != NULL) {
        if (strcmp(args[arg_idx], ">") == 0) {
            // Output redirection in overwrite mode
            out_fd = open(args[arg_idx + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("Failed to open output file");
                return;
            }
            args[arg_idx] = NULL;  // Nullify '>' so it's not printed
            break;
        } else if (strcmp(args[arg_idx], ">>") == 0) {
            // Output redirection in append mode
            out_fd = open(args[arg_idx + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (out_fd == -1) {
                perror("Failed to open output file");
                return;
            }
            args[arg_idx] = NULL;  // Nullify '>>' so it's not printed
            break;
        }
        arg_idx++;
    }

    // Redirect stdout if necessary
    int saved_stdout = -1;
    if (out_fd != STDOUT_FILENO) {
        saved_stdout = dup(STDOUT_FILENO);  // Save current stdout
        dup2(out_fd, STDOUT_FILENO);        // Redirect stdout to the file
    }

    // Print each argument after handling environment variables and removing quotes
    for (int i = 1; args[i] != NULL; i++) {
        // Handle environment variables (e.g., $PWD)
        char *processed_arg = handle_env_variables(args[i]);

        // Remove surrounding quotes
        processed_arg = remove_quotes(processed_arg);

        // Print the processed argument
        printf("%s", processed_arg);
        if (args[i + 1] != NULL) {
            printf(" ");  // Add space between arguments
        }
    }
    printf("\n");

    // Restore stdout if it was redirected
    if (out_fd != STDOUT_FILENO) {
        dup2(saved_stdout, STDOUT_FILENO);  // Restore original stdout
        close(out_fd);                      // Close the file descriptor
        close(saved_stdout);                // Close saved stdout descriptor
    }
}


// Built-in function to handle 'cd' command
void quash_cd(char **args) {
    if (args[1] == NULL || strcmp(args[1], "~") == 0) {
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "HOME not set\n");
        } else if (chdir(home) != 0) {
            perror("chdir");
        }
    } else if (strcmp(args[1], "..") == 0) {
        if (chdir("..") != 0) {
            perror("chdir");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("chdir");
        }
    }
}

// Function to execute external commands
void execute_external_command(char **args, int background) {
    pid_t pid = fork();
    
    if (pid == 0) {  // Child process
        if (execvp(args[0], args) == -1) {
            perror("execvp");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork failed");
    } else {  // Parent process
        if (background) {
            add_job(pid, args[0]);
            printf("Background job started: [%d] %d %s\n", job_count, pid, args[0]);
        } else {
            waitpid(pid, NULL, 0);  // Wait for foreground process to finish
        }
    }
}

// Function to add a job to the job list
void add_job(pid_t pid, char *command) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].job_id = job_count + 1;
        jobs[job_count].pid = pid;
        strcpy(jobs[job_count].command, command);
        jobs[job_count].active = 1;
        job_count++;
    }
}

// Function to print currently running jobs
void print_jobs() {
    int active_jobs = 0;

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            printf("[%d] %d %s &\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);
            active_jobs++;
        }
    }

    if (active_jobs == 0) {
        printf("No jobs running\n");
    }
}

// Function to check for completed background jobs
void check_background_jobs() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);  // Check if the job has finished
            if (result == 0) {
                // The job is still running (waitpid returns 0)
                continue;
            } else if (result == jobs[i].pid) {
                // The job has finished (waitpid returns the PID)
                printf("Completed: [%d] %d %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);
                jobs[i].active = 0;  // Mark the job as inactive
            } else if (result == -1) {
                // An error occurred (this shouldn't usually happen unless the job doesn't exist)
                perror("waitpid");
            }
        }
    }
}

// Function to remove a job from the job list (not used but could be useful)
// Function to remove a job from the job list by marking it inactive
// Function to remove a job from the job list by marking it inactive
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].active = 0;
            break;
        }
    }
}

// Function to kill a process by PID or JOBID
void kill_process(char **args) {
    if (args[1] == NULL) {
        printf("Usage: kill <PID> or kill %%<JOBID>\n");
        return;
    }

    if (args[1][0] == '%') {
        // Parse as job ID if it starts with '%'
        int job_id = atoi(args[1] + 1);  // Extract job ID from string after '%'
        if (job_id > 0) {
            kill_job_by_id(job_id);
        } else {
            printf("Invalid job ID: %s\n", args[1]);
        }
    } else {
        // Parse as PID if it doesn't start with '%'
        int is_digit = 1;
        for (int i = 0; args[1][i] != '\0'; i++) {
            if (!isdigit(args[1][i])) {
                is_digit = 0;
                break;
            }
        }
        if (is_digit) {
            pid_t pid = atoi(args[1]);
            if (kill(pid, SIGKILL) == 0) {  // Use SIGKILL for immediate termination
                int status;
                waitpid(pid, &status, 0);  // Wait for process termination
                printf("Process %d terminated\n", pid);
                remove_job(pid);  // Mark the job as inactive
            } else {
                perror("kill");
            }
        } else {
            printf("Invalid PID: %s\n", args[1]);
        }
    }
}

// Function to kill a job by job ID
void kill_job_by_id(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id && jobs[i].active) {
            if (kill(jobs[i].pid, SIGKILL) == 0) {  // Use SIGKILL to ensure termination
                int status;
                waitpid(jobs[i].pid, &status, 0);  // Wait for process termination
                printf("Job [%d] (PID %d) terminated\n", job_id, jobs[i].pid);
                jobs[i].active = 0;  // Mark job as inactive
            } else {
                perror("kill");
            }
            return;
        }
    }
    printf("Job ID %d not found\n", job_id);
}
// Function to handle the export command
void export_variable(char *arg) {
    char *delimiter = strchr(arg, '=');
    if (delimiter == NULL) {
        printf("Usage: export VAR=VALUE\n");
        return;
    }

    // Split the argument into VAR and VALUE
    *delimiter = '\0';  // Temporarily terminate the string at '='
    char *var_name = arg;
    char *value = delimiter + 1;

    // Set the environment variable
    if (setenv(var_name, value, 1) == 0) {
        printf("Exported: %s=%s\n", var_name, value);
    } else {
        perror("export failed");
    }

    *delimiter = '=';  // Restore the original argument string
}
// fucniton to handle find 
void handle_find(char **args) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed for find command");
        return;
    } else if (pid == 0) {  // Child process
        // If no specific path is provided, default to the current directory
        char *path = (args[1] != NULL) ? args[1] : ".";
        char *pattern = (args[2] != NULL) ? args[2] : "*";

        // Execute the find command with provided path and pattern
        execlp("find", "find", path, "-name", pattern, NULL);

        // If exec fails
        perror("Exec failed for find");
        _exit(1);
    } else {  // Parent process
        wait(NULL);  // Wait for child to finish
    }
}

// fucntion to ahndle grep

char* remove_quotesforgrep(char* arg) {
    size_t len = strlen(arg);
    if (len > 1 && ((arg[0] == '\'' && arg[len - 1] == '\'') || (arg[0] == '\"' && arg[len - 1] == '\"'))) {
        // Remove the first and last character (quotes)
        arg[len - 1] = '\0';  // Remove the closing quote
        return arg + 1;       // Skip the opening quote
    }
    return arg;  // Return the argument unchanged if no quotes are found
}

void handle_grep(char **args) {
    // Debug: Print the arguments
    printf("Debug: Arguments passed to grep:\n");
    for (int i = 0; args[i] != NULL; i++) {
        printf("args[%d] = %s\n", i, args[i]);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed for grep command");
        return;
    } else if (pid == 0) {  // Child process
        // Execute grep with the provided args directly
        execvp("grep", args);

        // If exec fails, print an error
        perror("Exec failed for grep");
        _exit(1);
    } else {  // Parent process
        wait(NULL);  // Wait for the child process to finish
    }
}
//SOLVED CAT IN SEPRATE FILE AVGJEFNJKgknthkoiq4 o24h9-kporhkj
void handle_cat(char **args) {
    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;
    int append_mode = 0;  // Track if we're appending to the output file

    // First, parse args for redirection operators and setup input/output
    for (int i = 1; args[i] != NULL; i++) {
       printf("Debug: Current argument - %s\n", args[i]); 
        if (strcmp(args[i], "<") == 0) {  // Input redirection
            in_fd = open(args[i + 1], O_RDONLY);
            if (in_fd == -1) {
                perror("Failed to open input file");
                return;
            }
            printf("Debug: Input redirection set to file %s\n", args[i + 1]);
            // Shift arguments to remove "<" and filename
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
            append_mode = 1;
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