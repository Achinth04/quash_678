#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

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
    for (int i = 1; args[i] != NULL; i++) {
        // Handle environment variables (e.g., $pwd)
        char* processed_arg = handle_env_variables(args[i]);

        // Remove surrounding quotes
        processed_arg = remove_quotes(processed_arg);

        // Print the processed argument
        printf("%s ", processed_arg);
    }
    printf("\n");
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
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            printf("[%d] %d %s &\n", jobs[i].job_id, jobs[i].pid, jobs[i].command);
        }
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
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].active = 0;
            break;
        }
    }
}
