#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 100000

typedef struct {
    char *name;
    char *value;
    int exported;
} ShellVar;

ShellVar *variables = NULL;
int var_count = 0;

void set_variable(const char *name, const char *value, int exported) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            free(variables[i].value);
            variables[i].value = strdup(value);
            if (exported) {
                variables[i].exported = 1;
                setenv(name, value, 1);
            }
            return;
        }
    }
    variables = (ShellVar *) realloc(variables, (var_count + 1) * sizeof(ShellVar));
    variables[var_count].name = strdup(name);
    variables[var_count].value = strdup(value);
    variables[var_count].exported = exported;
    if (exported) {
        setenv(name, value, 1);
    }
    var_count++;
}

char* get_variable_value(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void export_variable(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            variables[i].exported = 1;
            setenv(variables[i].name, variables[i].value, 1);
            return;
        }
    }
    fprintf(stderr, "export: variable '%s' not found\n", name);
}

void free_variables() {
    for (int i = 0; i < var_count; i++) {
        free(variables[i].name);
        free(variables[i].value);
    }
    free(variables);
    variables = NULL;
    var_count = 0;
}

// Function to expand variables in a string
char* expand_variables(const char *input) {
    if (!input) return NULL;
    
    // First, calculate needed buffer size
    size_t output_size = 1; // Start with 1 for the null terminator
    char *var_name = NULL;
    int in_var = 0;
    size_t var_name_len = 0;
    
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '$' && input[i+1] != '\0' && input[i+1] != ' ') {
            // Start of variable
            in_var = 1;
            var_name_len = 0;
            var_name = (char *)realloc(var_name, var_name_len + 1);
            var_name[var_name_len] = '\0';
        } else if (in_var) {
            if (input[i] == ' ' || input[i] == '\0' || input[i] == '$' || 
                input[i] == '/' || input[i] == '>' || input[i] == '<') {
                // End of variable
                in_var = 0;
                var_name[var_name_len] = '\0';
                char *value = get_variable_value(var_name);
                output_size += value ? strlen(value) : 0;
                if (input[i] == '$') {
                    i--; // Reprocess the $ character
                } else {
                    output_size++; // For the delimiter or null
                }
            } else {
                // Inside variable name
                var_name_len++;
                var_name = (char *)realloc(var_name, var_name_len + 1);
                var_name[var_name_len - 1] = input[i];
                var_name[var_name_len] = '\0';
            }
        } else {
            // Normal character
            output_size++;
        }
    }
    
    // If we were still processing a variable at the end
    if (in_var) {
        var_name[var_name_len] = '\0';
        char *value = get_variable_value(var_name);
        output_size += value ? strlen(value) : 0;
    }
    
    // Now create the output string
    char *output = (char *)malloc(output_size);
    if (!output) {
        free(var_name);
        return NULL;
    }
    
    size_t pos = 0;
    in_var = 0;
    var_name_len = 0;
    
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (input[i] == '$' && input[i+1] != '\0' && input[i+1] != ' ') {
            // Start of variable
            in_var = 1;
            var_name_len = 0;
            var_name = (char *)realloc(var_name, var_name_len + 1);
            if (!var_name) {
                free(output);
                return NULL;
            }
            var_name[var_name_len] = '\0';
        } else if (in_var) {
            if (input[i] == ' ' || input[i] == '\0' || input[i] == '$' || 
                input[i] == '/' || input[i] == '>' || input[i] == '<') {
                // End of variable
                in_var = 0;
                var_name[var_name_len] = '\0';
                char *value = get_variable_value(var_name);
                if (value) {
                    strcpy(output + pos, value);
                    pos += strlen(value);
                }
                if (input[i] == '$') {
                    i--; // Reprocess the $ character
                } else {
                    output[pos++] = input[i]; // Copy the delimiter
                }
            } else {
                // Inside variable name
                var_name_len++;
                var_name = (char *)realloc(var_name, var_name_len + 1);
                if (!var_name) {
                    free(output);
                    return NULL;
                }
                var_name[var_name_len - 1] = input[i];
                var_name[var_name_len] = '\0';
            }
        } else {
            // Normal character
            output[pos++] = input[i];
        }
    }
    
    // If we were still processing a variable at the end
    if (in_var) {
        var_name[var_name_len] = '\0';
        char *value = get_variable_value(var_name);
        if (value) {
            strcpy(output + pos, value);
            pos += strlen(value);
        }
    }
    
    output[pos] = '\0';
    free(var_name);
    return output;
}

// Function to parse command line into tokens, handling quotes and redirection
char** tokenize_command(char *cmd, int *token_count) {
    if (!cmd || !token_count) return NULL;
    
    int capacity = 10;
    char **tokens = (char**)malloc(capacity * sizeof(char*));
    if (!tokens) return NULL;
    
    int count = 0;
    char *current_pos = cmd;
    char quote_char = 0;
    int in_token = 0;
    char *token_start = NULL;
    
    while (*current_pos != '\0') {
        // Handle quotes
        if ((*current_pos == '"' || *current_pos == '\'') && (!quote_char || quote_char == *current_pos)) {
            if (!quote_char) {
                // Start quoted token
                quote_char = *current_pos;
                if (!in_token) {
                    in_token = 1;
                    token_start = current_pos + 1;
                }
            } else {
                // End quoted token
                quote_char = 0;
                *current_pos = '\0';
                if (count >= capacity - 1) {
                    capacity *= 2;
                    tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                    if (!tokens) return NULL;
                }
                tokens[count++] = strdup(token_start);
                in_token = 0;
            }
        }
        // Handle redirection symbols separately
        else if (!quote_char && (*current_pos == '<' || *current_pos == '>' || 
                 (*current_pos == '2' && *(current_pos + 1) == '>'))) {
            if (in_token) {
                // End the current token
                *current_pos = '\0';
                if (count >= capacity - 1) {
                    capacity *= 2;
                    tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                    if (!tokens) return NULL;
                }
                tokens[count++] = strdup(token_start);
                in_token = 0;
            }
            
            // Handle 2> as a single token
            if (*current_pos == '2' && *(current_pos + 1) == '>') {
                if (count >= capacity - 1) {
                    capacity *= 2;
                    tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                    if (!tokens) return NULL;
                }
                tokens[count++] = strdup("2>");
                current_pos += 2;
                continue;
            }
            
            // Add the redirection symbol as a token
            char symbol[2] = {*current_pos, '\0'};
            if (count >= capacity - 1) {
                capacity *= 2;
                tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                if (!tokens) return NULL;
            }
            tokens[count++] = strdup(symbol);
        }
        // Handle spaces
        else if (!quote_char && *current_pos == ' ') {
            if (in_token) {
                // End the current token
                *current_pos = '\0';
                if (count >= capacity - 1) {
                    capacity *= 2;
                    tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                    if (!tokens) return NULL;
                }
                tokens[count++] = strdup(token_start);
                in_token = 0;
            }
        }
        // Handle pipe symbol
        else if (!quote_char && *current_pos == '|') {
            if (in_token) {
                // End the current token
                *current_pos = '\0';
                if (count >= capacity - 1) {
                    capacity *= 2;
                    tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                    if (!tokens) return NULL;
                }
                tokens[count++] = strdup(token_start);
                in_token = 0;
            }
            
            // Add the pipe symbol as a token
            if (count >= capacity - 1) {
                capacity *= 2;
                tokens = (char**)realloc(tokens, capacity * sizeof(char*));
                if (!tokens) return NULL;
            }
            tokens[count++] = strdup("|");
        }
        // Handle normal characters
        else {
            if (!in_token) {
                in_token = 1;
                token_start = current_pos;
            }
        }
        
        current_pos++;
    }
    
    // Handle the last token if any
    if (in_token) {
        if (count >= capacity - 1) {
            capacity *= 2;
            tokens = (char**)realloc(tokens, capacity * sizeof(char*));
            if (!tokens) return NULL;
        }
        tokens[count++] = strdup(token_start);
    }
    
    // Null-terminate the array
    tokens[count] = NULL;
    *token_count = count;
    
    return tokens;
}

// Function to extract command args (excluding redirection operators and their targets)
char** extract_command_args(char **tokens, int token_count, int *new_count) {
    if (!tokens || !new_count) return NULL;
    
    char **cmd_args = (char**)malloc((token_count + 1) * sizeof(char*));
    if (!cmd_args) return NULL;
    
    int cmd_count = 0;
    
    for (int i = 0; i < token_count && tokens[i] != NULL; i++) {
        // Skip redirection operators and their filenames
        if (strcmp(tokens[i], "<") == 0 || 
            strcmp(tokens[i], ">") == 0 || 
            strcmp(tokens[i], "2>") == 0) {
            if (i + 1 < token_count && tokens[i+1] != NULL) {
                i++; // Skip the filename too
            }
            continue;
        }
        
        cmd_args[cmd_count++] = strdup(tokens[i]);
    }
    
    cmd_args[cmd_count] = NULL;
    *new_count = cmd_count;
    
    return cmd_args;
}

// Function to check if a file is accessible for the given mode
int is_file_accessible(const char *path, int mode) {
    return access(path, mode) == 0;
}
int setup_redirection(char **tokens, int token_count, int in_parent) {
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    int original_stderr = dup(STDERR_FILENO);
    int success = 1;
    int flag=0;
    
    // First, identify and validate all redirections
    for (int i = 0; i < token_count && tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            flag=1;
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                fprintf(stderr, "syntax error near unexpected token `>'\n");
                success = 0;
                break;
            }
            
            // Check if the directory is writable
            char *dir_path = strdup(tokens[i+1]);
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                // If path is empty after removing filename, use "." (current dir)
                if (strlen(dir_path) == 0) {
                    free(dir_path);
                    dir_path = strdup(".");
                }
            } else {
                free(dir_path);
                dir_path = strdup(".");
            }
            
            if (!is_file_accessible(dir_path, W_OK)) {
                fprintf(stderr, "%s: Permission denied\n", tokens[i+1]);
                free(dir_path);
                success = 0;
                break;
            }
            
            free(dir_path);
            i++; // Skip the filename
        }
        // Input redirection
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                fprintf(stderr, "syntax error near unexpected token `<'\n");
                success = 0;
                break;
            }
            
            if (!is_file_accessible(tokens[i+1], R_OK)&& flag==0) {
                // Don't use perror here as we're just checking, not opening yet
                fprintf(stderr, "cannot access %s: No such file or directory\n", tokens[i+1]);
                success = 0;
                break;
            }
            i++; // Skip the filename
        }
        // Output redirection
        
        // Error redirection
        else if (strcmp(tokens[i], "2>") == 0) {
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                fprintf(stderr, "syntax error near unexpected token `2>'\n");
                success = 0;
                break;
            }
            
            // Check if the directory is writable
            char *dir_path = strdup(tokens[i+1]);
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                // If path is empty after removing filename, use "." (current dir)
                if (strlen(dir_path) == 0) {
                    free(dir_path);
                    dir_path = strdup(".");
                }
            } else {
                free(dir_path);
                dir_path = strdup(".");
            }
            
            if (!is_file_accessible(dir_path, W_OK)) {
                fprintf(stderr, "%s: Permission denied\n", tokens[i+1]);
                free(dir_path);
                success = 0;
                break;
            }
            
            free(dir_path);
            i++; // Skip the filename
        }
    }
    
    // If validation failed, don't proceed with actual redirection
    if (!success) {
        close(original_stdin);
        close(original_stdout);
        close(original_stderr);
        return 0;
    }
    
    // Special case: if we're in the parent process and just validating, 
    // return success without actually performing redirection
    if (in_parent && success) {
        close(original_stdin);
        close(original_stdout);
        close(original_stderr);
        return 1;
    }
    
    // First handle stderr redirection to capture any errors in other redirections
    for (int i = 0; i < token_count && tokens[i] != NULL && success; i++) {
        if (strcmp(tokens[i], "2>") == 0) {
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                // This shouldn't happen as we've already validated
                success = 0;
                break;
            }
            
            stderr_fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stderr_fd < 0) {
                perror(tokens[i+1]);
                success = 0;
                break;
            }
            
            if (dup2(stderr_fd, STDERR_FILENO) < 0) {
                perror("dup2");
                success = 0;
                close(stderr_fd);
                break;
            }
            
            close(stderr_fd);
            i++; // Skip the filename
        }
    }
    
    // Now handle input redirection
    for (int i = 0; i < token_count && tokens[i] != NULL && success; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                // This shouldn't happen as we've already validated
                success = 0;
                break;
            }
            
            stdin_fd = open(tokens[i+1], O_RDONLY);
            if (stdin_fd < 0) {
                 if (errno == ENOENT) {
                    fprintf(stderr, "cannot access %s: No such file or directory\n", tokens[i+1]);
                } else {
                    perror(tokens[i+1]);
                }
                success = 0;
                break;
            }
            
            if (dup2(stdin_fd, STDIN_FILENO) < 0) {
                perror("dup2");
                success = 0;
                close(stdin_fd);
                break;
            }
            
            close(stdin_fd);
            i++; // Skip the filename
        }
    }
    
    // Finally handle output redirection
    for (int i = 0; i < token_count && tokens[i] != NULL && success; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 >= token_count || tokens[i+1] == NULL) {
                // This shouldn't happen as we've already validated
                success = 0;
                break;
            }
            
            stdout_fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stdout_fd < 0) {
                perror(tokens[i+1]);
                success = 0;
                break;
            }
            
            if (dup2(stdout_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                success = 0;
                close(stdout_fd);
                break;
            }
            
            close(stdout_fd);
            i++; // Skip the filename
        }
    }
    
    // If an error occurred, restore the original file descriptors
    if (!success) {
        dup2(original_stdin, STDIN_FILENO);
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
    }
    
    close(original_stdin);
    close(original_stdout);
    close(original_stderr);
    
    return success;
}
int microshell_main(int argc, char *argv[]) {
    char buf[BUF_SIZE];
    pid_t pid;
    int last_status = 0;
    int has_error = 0;

    while (1) {
        printf("Nano Shell Prompt > ");
        fflush(stdout);

        if (fgets(buf, BUF_SIZE, stdin) == NULL) {
            break;
        }

        buf[strcspn(buf, "\n")] = 0;  // remove newline

        if (strlen(buf) == 0) continue;

        // Check for assignment (exactly: name=value)
        char *eq = strchr(buf, '=');
        if (eq != NULL && eq != buf && !strchr(buf, ' ')) {
            *eq = '\0';
            char *name = buf;
            char *value = eq + 1;
            set_variable(name, value, 0);
            last_status = 0;
            continue;
        }

        // Expand variables in the entire command line first
        char *expanded_buf = expand_variables(buf);
        if (!expanded_buf) {
            fprintf(stderr, "Memory allocation error\n");
            last_status = 1;
            has_error = 1;
            continue;
        }
        
        // Tokenize the command
        int token_count = 0;
        char **tokens = tokenize_command(expanded_buf, &token_count);
        if (!tokens || token_count == 0) {
            free(expanded_buf);
            if (tokens) free(tokens);
            continue;
        }
        
        // Extract command arguments (excluding redirection tokens)
        int cmd_count = 0;
        char **cmd_args = extract_command_args(tokens, token_count, &cmd_count);
        if (!cmd_args || cmd_count == 0) {
            for (int i = 0; i < token_count; i++) {
                if (tokens[i]) free(tokens[i]);
            }
            free(tokens);
            free(expanded_buf);
            continue;
        }
        
        // Save original file descriptors
        int original_stdin = dup(STDIN_FILENO);
        int original_stdout = dup(STDOUT_FILENO);
        int original_stderr = dup(STDERR_FILENO);
        
        // Handle built-in commands
        if (strcmp(cmd_args[0], "pwd") == 0) {
            // Validate redirections first, but don't perform them yet
            if (!setup_redirection(tokens, token_count, 1)) {
                last_status = 1;
                has_error = 1;
            } else {
                // Now actually perform redirections
                if (setup_redirection(tokens, token_count, 0)) {
                    char cwd[BUF_SIZE];
                    if (getcwd(cwd, sizeof(cwd)) != NULL) {
                        printf("%s\n", cwd);
                        last_status = 0;
                    } else {
                        perror("pwd");
                        last_status = 1;
                        has_error = 1;
                    }
                } else {
                    // This should never happen since we already validated
                    last_status = 1;
                    has_error = 1;
                }
            }
        } else if (strcmp(cmd_args[0], "echo") == 0) {
            // Validate redirections first, but don't perform them yet
            if (!setup_redirection(tokens, token_count, 1)) {
                last_status = 1;
                has_error = 1;
            } else {
                // Now actually perform redirections
                if (setup_redirection(tokens, token_count, 0)) {
                    // Output the echo arguments
                    for (int j = 1; cmd_args[j] != NULL; j++) {
                        printf("%s", cmd_args[j]);
                        if (cmd_args[j + 1] != NULL) printf(" ");
                    }
                    printf("\n");
                    fflush(stdout);
                    last_status = 0;
                } else {
                    // This should never happen since we already validated
                    last_status = 1;
                    has_error = 1;
                }
            }
        } else if (strcmp(cmd_args[0], "cd") == 0) {
            if (cmd_args[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n");
                last_status = 1;
                has_error = 1;
            } else if (chdir(cmd_args[1]) != 0) {
                fprintf(stderr, "cd: %s: No such file or directory\n", cmd_args[1]);
                last_status = 1;
                has_error = 1;
            } else {
                last_status = 0;
            }
        } else if (strcmp(cmd_args[0], "export") == 0) {
            if (cmd_args[1] == NULL) {
                fprintf(stderr, "export: missing argument\n");
                last_status = 1;
                has_error = 1;
            } else {
                export_variable(cmd_args[1]);
                last_status = 0;
            }
        } else if (strcmp(cmd_args[0], "exit") == 0) {
            printf("Good Bye\n");
            for (int j = 0; j < cmd_count; j++) {
                if (cmd_args[j]) free(cmd_args[j]);
            }
            free(cmd_args);
            
            for (int j = 0; j < token_count; j++) {
                if (tokens[j]) free(tokens[j]);
            }
            free(tokens);
            
            free(expanded_buf);
            free_variables();
            close(original_stdin);
            close(original_stdout);
            close(original_stderr);
            return has_error ? 1 : last_status;
        } else {
            // Validate redirections first
            if (!setup_redirection(tokens, token_count, 1)) {
                last_status = 1;
                has_error = 1;
            } else {
                // Fork and execute the command
                pid = fork();
                if (pid == 0) {
                    // Child process - set up redirections
                    if (!setup_redirection(tokens, token_count, 0)) {
                        exit(1); // Exit with error if redirection fails
                    }
                    
                    // Set environment variables
                    for (int j = 0; j < var_count; j++) {
                        if (variables[j].exported) {
                            setenv(variables[j].name, variables[j].value, 1);
                        }
                    }
                    
                    // Execute command
                    execvp(cmd_args[0], cmd_args);
                    
                    // If execvp returns, an error occurred
                    fprintf(stderr, "%s: command not found\n", cmd_args[0]);
                    exit(127);  // standard shell convention for "command not found"
                } else if (pid > 0) {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);
                    if (WIFEXITED(status)) {
                        last_status = WEXITSTATUS(status);
                        if (last_status != 0) {
                            has_error = 1;
                        }
                    } else {
                        last_status = 1;
                        has_error = 1;
                    }
                } else {
                    // Fork failed
                    perror("Fork failed");
                    last_status = 1;
                    has_error = 1;
                }
            }
        }
        
        // Restore original file descriptors
        dup2(original_stdin, STDIN_FILENO);
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
        close(original_stdin);
        close(original_stdout);
        close(original_stderr);

        // Free argument arrays
        for (int j = 0; j < cmd_count; j++) {
            if (cmd_args[j]) free(cmd_args[j]);
        }
        free(cmd_args);
        
        for (int j = 0; j < token_count; j++) {
            if (tokens[j]) free(tokens[j]);
        }
        free(tokens);
        
        free(expanded_buf);
    }

    free_variables();
    return has_error ? 1 : last_status;  // Return error if any errors occurred
}
