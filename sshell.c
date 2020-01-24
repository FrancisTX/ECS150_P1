#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512

int pipe_returns[20];
int start_index = 0;

/* Contains all commands that need to pipe */
struct commands_to_pipe {
    const char *command[CMDLINE_MAX];
};

struct Dir_Stack {

    char cur_directory[CMDLINE_MAX];
    int count;
    struct Dir_Stack* next;
};

/* Single linked list stores dirs */
struct Dir_Stack* add_cur_stack(struct Dir_Stack* head, char new_path[]) {
    head->next = (struct Dir_Stack*) malloc(sizeof(struct Dir_Stack));
    head->next->count = head->count + 1;
    head = head->next;
    strcpy(head->cur_directory, new_path);
    head->next = NULL;
    return head;
}

/* Execute regular single command*/
void ExecuteCommand(char* parsed_command[CMDLINE_MAX], char cmd[CMDLINE_MAX])
{
    int status;

    if (fork() != 0) {
        /* parent */
        waitpid(-1, &status, 0);
        /* print status */
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
    } else {
        /* child */
        execvp(parsed_command[0], parsed_command);
        /* If execute command failed, go to following codes */
        fprintf(stderr, "Error: command not found\n");
        exit(1);
    }
}

/* pipe helper function that connects file descriptor */
//https://stackoverflow.com/questions/8082932/connecting-n-commands-with-pipes-in-a-shell
int PipeExecutor(int input_fd, int output_fd, struct commands_to_pipe cmd, int if_err_re) {
    pid_t pid;
    int status;

    if ((pid = fork()) != 0) {
        /* parent */
        waitpid(-1, &status, 0);
        /* Store the return value of child */
        pipe_returns[start_index] = WEXITSTATUS(status);
        start_index++;
    } else {
        /* child */
        if (input_fd != 0) {
            /* Close stdin and open output of previous command */
            /* Except the first command */
            dup2(input_fd, 0);
            close(input_fd);
        }

        if (if_err_re == 1) {
            /* If |&, close stderr and open the input of the next command */
            dup2(output_fd, 2);
            close(output_fd);
        } else {
            /* If |, close stdout and open the input of the next command */
            if (output_fd != 1) {
                dup2(output_fd, 1);
                close(output_fd);
            }
        }

        /* Execute the current command */
        execvp(cmd.command[0], (char *const *)cmd.command);
    }

    return pid;
}

/* Split the commands need to pipe and execute pipe */
int ExecutePipe(char* parsed_command[CMDLINE_MAX], int num_pipe) {
    /* No more than 4 pipes required for this assignment. Capable for more pipes */
    struct commands_to_pipe commands[6];
    int index = 0, no_command = 0, i = 0;
    /* This array checks if the command is followed by |& */
    /* If so, 1. Else, 0 */
    int err_redirect[num_pipe + 1];

    /* Store each command individually in the struct */
    while (parsed_command[index] != NULL) {
        if (!strcmp(parsed_command[index], "|")) {
            /* The current command is complete, the command is followed by | */
            err_redirect[no_command] = 0;
            no_command++;
            i = 0;
        } else if (!strcmp(parsed_command[index], "|&")) {
            /* The current command is complete, the command is followed by |& */
            err_redirect[no_command] = 1;
            no_command++;
            i = 0;
        } else {
            /* Append the word to current command */
            commands[no_command].command[i] = parsed_command[index];
            i++;
        }
        index++;
    }

    /* input_fd stores the input that we should connect to */
    /* Initial should be zero because it's the first command */
    int input_fd = 0, j, fd[2];

    for (j = 0; j < num_pipe; j++) {
        pipe(fd);

        /* Parameter output_fd is always be input of next command */
        PipeExecutor(input_fd, fd[1], commands[j], err_redirect[j]);
        close(fd[1]);
        input_fd = fd[0];
    }

    /* Pipe the last command */
    int status;

    if (fork() != 0) {
        /* Parent */
        waitpid(-1, &status, 0);
        pipe_returns[start_index] = WEXITSTATUS(status);
        start_index++;
    } else {
        /* Child */
        /* Close stdin and open previous output */
        if (input_fd != 0)
            dup2(input_fd, 0);

        execvp(commands[j].command[0], (char *const *) (commands[j].command));
    }

    return status;
}

/* The function gets redirection and connect the file descriptor */
void OutputRedirection(char* parsed_command[CMDLINE_MAX], char cmd[CMDLINE_MAX], int saved_stdout, int saved_stderr, int num_pipe) {

    char* Output_Command[CMDLINE_MAX];
    int index = 0;
    int fd;
    int if_err_re = 0;

    /* Find the command before redirection */
    while (strcmp(parsed_command[index], ">") && strcmp(parsed_command[index], ">&")) {
        Output_Command[index] = parsed_command[index];
        index++;
    }

    /* Check if >& exits */
    if (!strcmp(parsed_command[index], ">&"))
        if_err_re = 1;

    Output_Command[index] = NULL;

    fd = open (parsed_command[index + 1], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);

    if (fd <= -1) {
        fprintf(stderr, "Error: cannot open output file\n");
        return;
    }

    if (if_err_re) {
        /* If >&, close stderr and connect to file */
        dup2(fd, 2);
        close(fd);
    } else {
        /* If >, close stdout and connect to file */
        dup2(fd,1);
        close(fd);
    }

    if (num_pipe > 0) {
        /* If pipe exits, do piping */
        ExecutePipe(Output_Command, num_pipe);
    } else {
        /* Execute regular command */
        ExecuteCommand(Output_Command, cmd);
    }

    if (if_err_re) {
        /* Connect origin stderr back */
        dup2(saved_stderr, 2);
    } else {
        /* Connect origin stdout back */
        dup2(saved_stdout, 1);
    }
}

/* Scan the cmd line and split it with space, pipe, and redirection */
int ParseCommand(char* command, char* parsed_command[CMDLINE_MAX], int* num_pipe, int* with_redirection)
{
    int index = 0;
    int word_length = 0;
    char cur_word[CMDLINE_MAX][CMDLINE_MAX];

    int i = 0;

    /* Scan the cmd string */
    for (i = 0; ; i++){
        /* Reach the end of the cmd string. Store last word if exits */
        if (command[i] == '\0' && word_length == 0) {
            if (!strcmp(parsed_command[index - 1], "|") || !strcmp(parsed_command[index - 1], "|&")) {
                fprintf(stderr, "Error: missing command\n");
                return 1;
            } else if (!strcmp(parsed_command[index - 1], ">") || !strcmp(parsed_command[index - 1], ">&")) {
                fprintf(stderr, "Error: no output file\n");
                return 1;
            }

            break;
        }

        if (command[i] == '\0' && word_length > 0) {
            cur_word[index][word_length] = 0;
            parsed_command[index] = cur_word[index];
            index++;
            break;
        }

        /* Reach a space. Store current word if exits */
        if (command[i] == ' ' && word_length > 0) {
            cur_word[index][word_length++] = 0;
            parsed_command[index] = cur_word[index];
            word_length = 0;
            index++;
        } else if (command[i] == '|' && command[i + 1] == '&') {
            /* Reach |&. Store current word and |& if exits. Else store |& only */
            *num_pipe = *num_pipe + 1;
            if (word_length > 0) {
                cur_word[index][word_length] = 0;
                parsed_command[index] = cur_word[index];
                index++;
                word_length = 0;
            }
            cur_word[index][0] = '|';
            cur_word[index][1] = '&';
            parsed_command[index] = cur_word[index];
            i++;
            index++;
        } else if (command[i] == '|') {
            /* Check if > goes before | */
            if (*with_redirection > 0) {
                fprintf(stderr, "Error: mislocated output redirection\n");
                return 1;
            }

            /* Reach |. Store current word and | if exits. Else store | only */
            *num_pipe = *num_pipe + 1;
            if (word_length > 0) {
                cur_word[index][word_length] = 0;
                parsed_command[index] = cur_word[index];
                index++;
                word_length = 0;
            }
            cur_word[index][0] = '|';
            parsed_command[index] = cur_word[index];
            index++;
        } else if (command[i] == '>' && command[i + 1] == '&') {
            /* Reach >&. Store current word and >& if exits. Else store >& only */
            *with_redirection = *with_redirection + 1;
            if (word_length > 0) {
                cur_word[index][word_length] = 0;
                parsed_command[index] = cur_word[index];
                index++;
                word_length = 0;
            }
            cur_word[index][0] = '>';
            cur_word[index][1] = '&';
            parsed_command[index] = cur_word[index];
            i++;
            index++;
        } else if (command[i] == '>') {
            /* Reach >. Store current word and > if exits. Else store > only */
            *with_redirection = *with_redirection + 1;
            if (word_length > 0) {
                cur_word[index][word_length] = 0;
                parsed_command[index] = cur_word[index];
                index++;
                word_length = 0;
            }
            cur_word[index][0] = '>';
            parsed_command[index] = cur_word[index];
            index++;
        } else if (command[i] != ' '){
            /* Add the char to the end of current word */
            cur_word[index][word_length++] = command[i];
        } else {
            //Ignore all extra white space
        }
    }

    parsed_command[index] = NULL;

    /* Error missing command */
    if (!strcmp(parsed_command[0], "|") || !strcmp(parsed_command[0], "|&") || !strcmp(parsed_command[0], ">") || !strcmp(parsed_command[0], ">&")) {
        fprintf(stderr, "Error: missing command\n");
        return 1;
    }

    /* Error too many process arguments */
    if (!strcmp(parsed_command[0], "ls") && *num_pipe == 0 && *with_redirection == 0 && index > 3) {
        fprintf(stderr, "Error: too many process arguments\n");
        return 1;
    }

    return 0;
}

int ChangeDirectory(char* parsed_cmd[CMDLINE_MAX], char cur_dir[CMDLINE_MAX], struct Dir_Stack* head)
{
    char new_dir[CMDLINE_MAX];

    if (!strcmp(parsed_cmd[1], "..")) {
        /* cd .. */
        int count = 0;

        /* Scan the directory and take away the folder after last / */
        for (int i = strlen(cur_dir) - 1; i >= 0; i--) {
            if (cur_dir[i] == '/') {
                count = i;
                break;
            }
        }

        cur_dir[count] = 0;
        strcpy(new_dir, cur_dir);
    } else {
        /* cd dir */
        strcpy(new_dir, parsed_cmd[1]);
    }
    strcpy(head ->cur_directory, new_dir);

    /* cd failure */
    if (chdir(new_dir) != 0) {
        fprintf(stderr, "Error: no such directory\n");
        return 1;
    }

    return 0;
}

int main(void)
{
    char cmd[CMDLINE_MAX];
    char cur_dir[CMDLINE_MAX];
    int saved_stdout = dup(1);
    int saved_stderr = dup(2);

    struct Dir_Stack* head;
    struct Dir_Stack* new_stack;
    /* Always store the cur_dir as the first element in Dir_Stack */
    getcwd(cur_dir, CMDLINE_MAX);
    head = NULL;

    new_stack = (struct Dir_Stack*)malloc(sizeof(struct Dir_Stack*));
    strcpy(new_stack->cur_directory, cur_dir);
    new_stack -> count = 1;
    head = new_stack;
    head->next = NULL;

    while (1) {
        char *nl;
        int with_redirection = 0;
        int num_pipe = 0;
        int status;
        char cmd_cpy[CMDLINE_MAX];
        char current_directory[CMDLINE_MAX];
        char *parsed_command[CMDLINE_MAX];

        /* Print prompt */
        printf("sshell$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        /* Builtin command */
        /* exit */
        if (!strcmp(cmd, "exit")) {
            fprintf(stderr, "Bye...\n");
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
            break;
        }

        /* Parse command */
        strcpy(cmd_cpy, cmd);
        int is_error = ParseCommand(cmd_cpy, parsed_command, &num_pipe, &with_redirection);

        if (is_error != 0)
            continue;

        if (!strcmp(cmd, "pwd")) {
            /* pwd */
            getcwd(current_directory, CMDLINE_MAX);
            printf("%s\n", current_directory);
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
        } else if (!strcmp(parsed_command[0], "cd")) {
            /* cd */
            int return_value = ChangeDirectory(parsed_command, cur_dir, head);
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, return_value);
        } else if (!strcmp(parsed_command[0], "dirs")) {
            //dirs
            if (head == NULL){
                fprintf(stderr, "Error: directory stack empty\n");
            }else {
                if (head->count == 1) {
                    printf("%s\n", head->cur_directory);
                }else {
                    struct Dir_Stack *curp = new_stack;
                    for (int i = head->count; i >= 1; i--) {
                        int a = 0;
                        curp = new_stack;
                        while (a != i - 1) {
                            curp = curp->next;
                            a++;
                        }
                        printf("%s\n", curp->cur_directory);
                    }
                }
            }
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
        } else if (!strcmp(parsed_command[0], "popd")) {
            //popd
            int count = head->count;
            head = new_stack;
            while (1){
                int countloop = 0;
                if (countloop == count - 2 ){
                    head -> next = NULL;
                    break;
                }else {
                    head = head -> next;
                    countloop++;
                }
            }
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
            chdir(head -> cur_directory);
        } else if (!strcmp(parsed_command[0], "pushd")) {
            //pushd
            char* push_path;
            push_path = cur_dir;
            strcat(push_path,"/");
            strcat(push_path, parsed_command[1]);

            if (chdir(push_path) != 0) {
                fprintf(stderr,"Error: no such directory\n");
                fprintf(stderr, "+ completed '%s' [1]\n", cmd);
            } else {
                head = add_cur_stack(head, push_path);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
            }
        } else {
            if (with_redirection) {
                /* Execute command with output redirection */
                OutputRedirection(parsed_command, cmd, saved_stdout, saved_stderr, num_pipe);
                with_redirection = 0;
            } else if (num_pipe > 0) {
                /* Execute command pipe */
                ExecutePipe(parsed_command, num_pipe);
                fprintf(stderr, "+ completed '%s' ", cmd);
                for (int i = start_index - num_pipe - 1; i < start_index; i++)
                    printf("[%d]", pipe_returns[i]);
                fprintf(stderr, "\n");
            } else {
                /* Execute other commands */
                ExecuteCommand(parsed_command, cmd);
            }
        }
    }
    struct Dir_Stack* free_stack;
    while ((free_stack = head) != NULL){
	    head = head->next;
	    free (free_stack);
    }
   

    return EXIT_SUCCESS;
}
