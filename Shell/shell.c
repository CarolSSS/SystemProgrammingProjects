/**
 * shell
 * CS 241 - Spring 2021
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include "sstring.h"

#include <assert.h>
#include <string.h>
#include <unistd.h> 
#include <signal.h>
#include <fcntl.h>
#include <ctype.h> 


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


typedef struct process {
    char *command;
    pid_t pid;
} process;

static vector* history_stor;
static vector* cmds_stor;
static vector* vec_process;
static char * curr_directory;
static char * command_n;
static int argc_s;
static char** argv_s;


typedef struct process process_t;


void sigint_handler();
void delete_from_his(vector* hist);
int exec_shell(char * cmd_input);


process_t * constructor(char * cmd, pid_t pid) {
    char * tmp = strdup(cmd);
    process_t * ret = calloc(sizeof(process_t), 1);
    ret -> pid = pid;
    ret -> command = tmp;
    return ret;
}

void destructor(pid_t pid) {
    size_t size_process = vector_size(vec_process);
    size_t i = 0;
    for(i = 0; i < size_process; i++) {
        process_t * ret = (process*) vector_get(vec_process, i);
        if ( ret -> pid == pid ) {
            char * cmd = ret -> command;
            free(cmd);
            cmd = NULL;
            free(ret);
            ret = NULL;
            vector_erase(vec_process, i);
            break;
        }
    }
}



void sigint_handler() {
    size_t size_process = vector_size(vec_process);
    size_t i = 0;
    for(i = 0; i < size_process; i++) {
        process_t * ret = (process *)vector_get(vec_process, i);
        if(ret -> pid != getpgid(ret -> pid)){
            kill(ret -> pid, SIGKILL);
            destructor(ret -> pid);
        }
    }
    return;
}

int process_handler(int pid, int sig){
    // int pid = atoi(pid1 + 5);
    int flag = 0;
    for (size_t i = 0; i < vector_size(vec_process); i++) {
        process_t * curr = (process*)vector_get(vec_process, i);
        if (curr -> pid == pid) { 
            flag = 1;
            if (sig == SIGKILL) { 
                kill(pid, SIGKILL);
                print_killed_process(pid, curr -> command);
                destructor(pid);
            } else if (sig == SIGSTOP){
                kill(pid, SIGSTOP);
                print_stopped_process(pid, curr -> command);
            } else {
                kill(pid, SIGCONT);
                print_continued_process(pid, curr -> command);
            }
            return 1;
        }
    }
    print_no_process_found(pid);
    return -1;
}


void delete_from_his(vector* hist){
    vector_pop_back(hist);
    return;
}

int ps_cmd() {
    print_process_info_header();
    process* shell_ = constructor(argv_s[0], getpid());
    vector_insert(vec_process, 0, shell_);
    if(vec_process == NULL) {
        return 1;
    }
    if (vector_size(vec_process) == 0) {
        return 1;
    }

    for (size_t i = 0; i < vector_size(vec_process); i++) {
        process_info * curr = calloc(sizeof(process_info), 1);
        curr -> pid = ((process*)vector_get(vec_process, i)) -> pid;
        curr -> command = calloc(strlen(((process*)vector_get(vec_process, i)) -> command) + 1, 1);
        char * cmd = ((process*)vector_get(vec_process, i)) -> command;
        strcpy(curr -> command, cmd);

        ////////
        char* status = malloc(1000);
        sprintf(status, "/proc/%d/status", curr -> pid);
        FILE* f_sta = fopen(status, "r");
        if (f_sta) {
            int threads1 = 0;
            int vsize1 = 0;
            char state1 = 0;
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), f_sta)) {
                if (buffer[strlen(buffer) - 1] == '\n') {
                    buffer[strlen(buffer) - 1] = '\0';
                }
                if (sscanf(buffer, "Threads: %d", &threads1) == 1) {
                    curr -> nthreads = threads1;
                    
                } else if (sscanf(buffer, "State:	%c (sleeping)", &state1) == 1) { 
                    curr -> state = state1;

                } else if (sscanf(buffer, "VmSize: %d KB", &vsize1) == 1) {
                    curr -> vsize = vsize1;
                }
            }
            curr -> start_str = NULL;
            curr -> time_str = NULL;
        }
        fclose(f_sta);
        free(status);

        ////////
        char* stat = malloc(1000);
        sprintf(stat, "/proc/%d/stat", curr -> pid);
        f_sta = fopen(stat, "r");
        if (!f_sta) {
            print_script_file_error();
            exit(1);
        } 
        char buffer[1000];
        unsigned long utime;
        unsigned long long starttime = 0;
        fgets(buffer, sizeof(buffer), f_sta);
        char* curr_time = strtok(buffer, " ");
        for (size_t i = 0; i <= 22; i++) {
            if(i == 14) {
                utime = atol(curr_time);
            } else if (i == 15) {
                utime = (utime + atol(curr_time)) / sysconf(_SC_CLK_TCK);
            } else if (i == 22) {
                starttime = atoll(curr_time) / sysconf(_SC_CLK_TCK);
            }
            curr_time = strtok(NULL, " ");
        }
        
        fclose(f_sta);
        free(stat);

        // time
        f_sta = fopen("/proc/stat", "r");
        if (!f_sta) {
            print_script_file_error();
            exit(1);
        } 
        unsigned long long bt = 0;
        while (fgets(buffer, sizeof(buffer), f_sta)) {
            if (sscanf(buffer, "btime %llu", &bt) == 1) {
                break;
            }
        }
        fclose(f_sta);

        time_t result_time = starttime + bt;
        struct tm* start_time = localtime(&result_time);
        time_struct_to_string(buffer, sizeof(buffer), start_time);
        curr -> start_str = strdup(buffer);
        
        execution_time_to_string(buffer,  sizeof(buffer),  utime / 60,  utime % 60);
        curr -> time_str = strdup(buffer);

        print_process_info(curr);

        free(curr -> time_str);
        free(curr -> start_str);
        free(curr -> command);
        free(curr);
    }
    vector_erase(vec_process, 0);
    return 1;
}


int pt1_external(char * cmd_input, char * file, int flag) {
    fflush(stdout);
    pid_t pid = fork();
    int status;

    if (pid < 0) { 
        print_fork_failed();
        exit(1);
    } else if (pid == 0) {
        if (cmd_input[strlen(cmd_input)-1] == '&') {
            cmd_input[strlen(cmd_input)-1] = '\0';
        }
        fflush(stdin);
        if (flag == 1) {
            close(1);
            FILE* curr_output = fopen(file, "w+");
            if (!curr_output) {
                print_script_file_error();
                exit(1);
            }
            if (fileno(curr_output)!= 1) {
                exit(1);
            }

        } else if (flag == 2) {
            close(1);
            FILE* curr_output = fopen(file, "a+");
            if (!curr_output) {
                print_script_file_error();
                exit(1);
            }
            if (fileno(curr_output)!= 1) {
                exit(1);
            }

        } else if (flag == 3) {
            close(0);
            FILE* curr_output = fopen(file, "r");
            if (!curr_output) {
                print_script_file_error();
                exit(1);
            }
  
        }
        vector* cmds = string_vector_create();
        char* tmp = strtok(cmd_input, " ");
        vector_push_back(cmds, tmp);
        while (tmp != NULL) {
            tmp = strtok(NULL, " ");
            vector_push_back(cmds, tmp);
        }
        char * run_cmds[vector_size(cmds) + 1];
        for(size_t i = 0; i < vector_size(cmds); i++) {
            run_cmds[i] = (char*) vector_get(cmds,i);
        }
        execvp(run_cmds[0], run_cmds);

        vector_destroy(cmds);
        print_exec_failed(cmd_input);
        exit(1);
    } else {
        process_t * curr_process = constructor(cmd_input, pid);
        vector_push_back(vec_process, curr_process);
        print_command_executed(pid);
        //////////////////
        if (cmd_input[strlen(cmd_input) - 1] == '&') {
            if (setpgid(pid, pid) == -1) { 
                print_setpgid_failed();
                return -1;
            }
        } else {
        //////////////////
            pid_t result = waitpid(pid, &status, 0);
            destructor(pid);
            if (result == -1) {
                print_wait_failed();
                exit(1);
            }
            if (WIFEXITED(status) && WEXITSTATUS(status)) {
                return -1;
            }
        }
    }
    return 0;
}



int exec_shell(char * cmd_input) {
    if (!strcmp(cmd_input, "!history")) {
        delete_from_his(history_stor); 
        delete_from_his(cmds_stor); 
        size_t i = 0;
        size_t size_vec_his = vector_size(history_stor);
        for(i = 0; i < size_vec_his; i++) {
            char * cmd1 = (char*) vector_get(history_stor,i);
            print_history_line((i + 1 -1), cmd1);
        }
        return 0;
        
    } else if (!strncmp(cmd_input, "#", 1)) {
        if (strlen(cmd_input) == 1) {
            print_invalid_index();
            return -1;
        }
        // delete_from_his(cmds_will_run); 
        delete_from_his(history_stor); 
        delete_from_his(cmds_stor); 
        char * num = cmd_input + 1;
        int index = atoi(num);
        if (index == 0 && strcmp(num, "0")) {
            print_invalid_index();
            return -1;
        } else if(index >= (int)vector_size(history_stor)) {
            print_invalid_index();
            return -1;
        } else {
            print_command(vector_get(history_stor, index));
            command_n = strdup(vector_get(history_stor, index));
            return 0;
        }
    
    } else if (!strncmp(cmd_input, "!", 1)) {
        delete_from_his(history_stor); 
        delete_from_his(cmds_stor); 
        int i = 0;
        int size_vec_his = vector_size(history_stor);
        char * output = cmd_input +1;
        int size_2 = strlen(cmd_input) - 1;
        for (i = (size_vec_his - 1); i >= 0; i--) {
            char * cmm = (char*) vector_get(history_stor, i);
            if(!strncmp(output, cmm, size_2)) {
                command_n = strdup(cmm);
                print_command(command_n);
                return 0;
            }
        }
        print_no_history_match();
        return -1;

    } else if(!strncmp(cmd_input, "cd", 2)) {
        char * dest = cmd_input + 3;
        char destination[strlen(dest)+1];
        strcpy(destination, dest);
        if(chdir(destination) == -1) {
            print_no_directory(destination);
            return -1;
        }
        strcpy(curr_directory, destination);
        curr_directory = getcwd(curr_directory, 50);
        // strcpy(curr_directory, destination);
        return 0;

    
    } else if (!strcmp(cmd_input, "exit")) {
        delete_from_his(history_stor); 
        delete_from_his(cmds_stor); 
        return 2;
    } else if (!strncmp(cmd_input, "kill", 4)) {
        if(strlen(cmd_input)< 6) {
            print_invalid_command(cmd_input);
            exit(-1);
        }
        int pid1 = atoi(cmd_input + 5);
        return process_handler(pid1, SIGKILL);

    } else if (!strncmp(cmd_input, "stop", 4)) {
        if(strlen(cmd_input)< 6) {
            print_invalid_command(cmd_input);
            exit(-1);
        }
        int pid1 = atoi(cmd_input + 5);
        return process_handler(pid1, SIGSTOP);


    } else if (!strncmp(cmd_input, "cont", 4)) {
        if(strlen(cmd_input + 4)< 2) {
            print_invalid_command(cmd_input);
            exit(-1);
        }
        int pid1 = atoi(cmd_input + 5);
        return process_handler(pid1, SIGCONT);

    } else if (!strcmp(cmd_input, "ps")) {
        return ps_cmd();

    } else {
        return pt1_external(cmd_input, NULL, 0);
    }
}


int logical_operator_detector(char * cmd_input) {
    if (strlen(cmd_input) < 1) {
        return 1;
    }
    int and_flag = 0;
    int or_flag = 0;
    int sep_flag = 0;
    int place_flag = 0;
    int append_flag = 0;
    int pipe_flag = 0;
    int index = 0;
    size_t i = 0;
    size_t size_vec_his = strlen(cmd_input) - 1;
    char * command_use = calloc((size_vec_his + 1),1);

    char* curr = strtok(command_use, " ");
    while (curr != NULL) {
        if (!strcmp(curr, "&&")) {
            and_flag = 1;
            break;
        } else if (!strcmp(curr, "||")) {
            or_flag = 1;
            break;
        } else if (!strcmp(curr, ";")) {
            sep_flag = 1;
            break;
        } else if (!strcmp(curr, ">")) {
            place_flag = 1;
            break;
        } else if (!strcmp(curr, ">>")) {
            append_flag = 1;
            break;
        } else if (!strcmp(curr, "<")) {
            pipe_flag = 1;
            break;
        }
        curr = strtok(NULL, " ");
    }
    free(command_use);
    command_use = NULL;

    for (i = 0; i < size_vec_his; i++) {
        if (cmd_input[i+1] == '&' && cmd_input[i] == '&') {
            and_flag = 1;
            index = i - 1; 
            break;
        } else if (cmd_input[i+1] == '|' && cmd_input[i] == '|') {
            or_flag = 1;
            index = i - 1;
            break;
        } else if (cmd_input[i] == ';') {
            sep_flag = 1;
            index = i - 1;
            break;
        }else if (cmd_input[i+1] == '>' && cmd_input[i] == '>') {
            append_flag = 1;
            index = i - 1;
            break;
        } else if (cmd_input[i] == '>') {
            place_flag = 1;
            index = i - 1;
            break;
        } else if (cmd_input[i] == '<') {
            pipe_flag = 1;
            index = i - 1;
            break;
        } else {
            continue;
        }
    }
    if (cmd_input[size_vec_his] == ';') {
        sep_flag = 1;
        index = size_vec_his - 1;
    }

    char * fir_cmd = NULL;
    char * sec_cmd = NULL;
    int status2 = 0;
    int status = 0; 

    if (and_flag == 0 && or_flag == 0 && sep_flag == 0 && place_flag == 0 && append_flag == 0 && pipe_flag == 0) {
        // printf("i m heare/n");
        return exec_shell(cmd_input);


    } else if (and_flag == 1) { 
        fir_cmd = strndup(cmd_input, index);
        fir_cmd[index] = '\0';
        status = exec_shell(fir_cmd);
        free(fir_cmd);
        if (status < 0) { 
            return -1;
        }
        sec_cmd = strndup(cmd_input + index + 4, size_vec_his - index - 2);
        status2 = exec_shell(sec_cmd);


    } else if(or_flag == 1) { 
        fir_cmd = strndup(cmd_input, index);
        fir_cmd[index] = '\0';
        status = exec_shell(fir_cmd);
        free(fir_cmd);
        if (status == 0) { 
            return 0;
        }

        sec_cmd = strndup(cmd_input + index + 4, size_vec_his - index - 2);
        //sec_cmd[size_vec_his - index - 2] = '\0';
        status2 = exec_shell(sec_cmd);

        
    } else if (sep_flag == 1) { 
        // fir_cmd = strok(cmd_input, "; ")
        fir_cmd = strndup(cmd_input, index + 1);
        fir_cmd[index + 1] = '\0';
        exec_shell(fir_cmd);
        free(fir_cmd);
        sec_cmd = strndup(cmd_input + index + 3, size_vec_his - index - 2);
        //sec_cmd[size_vec_his - index] = '\0';
        status2 = exec_shell(sec_cmd);


    // > operator
    } else if (place_flag == 1) {
        fir_cmd = strndup(cmd_input, index + 1);
        fir_cmd[index + 1] = '\0';
        sec_cmd = strndup(cmd_input + index + 3, size_vec_his - index - 2);
        //exec_shell(fir_cmd);
        
        pt1_external(fir_cmd, sec_cmd, 1);

    // >> operator
    } else if (append_flag == 1) {
        fir_cmd = strndup(cmd_input, index);
        fir_cmd[index] = '\0';
        sec_cmd = strndup(cmd_input + index + 4, size_vec_his - index - 2);
        // 1 for create write, 2 for append, 3 for read
        pt1_external(fir_cmd, sec_cmd, 2);


    // <  operator
    } else if (pipe_flag == 1) {
        fir_cmd = strndup(cmd_input, index + 1);
        fir_cmd[index + 1] = '\0';
        sec_cmd = strndup(cmd_input + index + 3, size_vec_his - index - 2);
        //exec_shell(fir_cmd);
        pt1_external(fir_cmd, sec_cmd, 3);
    }

    free(sec_cmd);
    return status2;
}

void eof_handler(ssize_t getline_) {
    if(getline_ == -1) {
        exit(1);
    }
}

int shell(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    
    int flag_prompt = 1;
    char line[1000];

    argc_s = argc;
    argv_s = argv;

    // Invalid command
    if ((argc != 1) && (argc != 3) && (argc != 5)) {
        print_usage();
        return 1;
    }  

    curr_directory = malloc(100);
    curr_directory = getcwd(curr_directory, 100);
    command_n = NULL;
    vec_process = shallow_vector_create();
    history_stor = string_vector_create();
    cmds_stor = string_vector_create();
    FILE*  history_file_ind = NULL;
    FILE*  command_file_ind = NULL;

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            char* full_path = get_full_path(argv[2]);
            history_file_ind = fopen(full_path, "a+");
            free(full_path);
            if(!history_file_ind) {
                print_script_file_error();

            }
            while (fgets(line, sizeof(line), history_file_ind)) {
                line[strlen(line) - 1] = '\0';  
                vector_push_back(history_stor, line);
            }
            if (argc == 5) {
                if (strcmp(argv[3], "-f") != 0) {
                    print_usage();
                    return 1;
                } else {
                    full_path = get_full_path(argv[4]);
                    command_file_ind = fopen(full_path, "r");
                    free(full_path);
                    if(!command_file_ind) {
                        print_script_file_error();
                    }
                }
            }

        } else if (strcmp(argv[1], "-f") == 0) {
            char* full_path = get_full_path(argv[2]);
            command_file_ind = fopen(full_path, "r");
            free(full_path);
            if(!command_file_ind) {
                print_script_file_error();
                return 1;
            }
            if (argc == 5) {
                if (strcmp(argv[3], "-f") == 0) {
                    full_path = get_full_path(argv[4]);
                    history_file_ind = fopen(argv[4], "a");
                    free(full_path);
                    if(!history_file_ind) {
                        print_script_file_error();
                    }
                    while (fgets(line, sizeof(line), history_file_ind)) {
                        line[strlen(line) - 1] = '\0';  
                        vector_push_back(history_stor, line);
                        vector_push_back(cmds_stor, line);
                    }
                } else {
                    print_usage();
                    exit(1);
                }
            }
        } else if (strcmp(argv[1], "-f") != 0 &&  strcmp(argv[1], "-f") != 0)  {
            print_usage();
            return 1;
        }
        
    }


    curr_directory = getcwd(curr_directory, 100);
    pid_t curr_pid = getpid();
    size_t size_ = 0;

    while (1) {
        char *cmd_run = NULL;
        if (!command_n && flag_prompt) {
            print_prompt(curr_directory, curr_pid);
        }
        if (command_file_ind) {
            ssize_t ret = getline(&cmd_run, &size_, command_file_ind);
            if(ret == -1) {
                flag_prompt = 0;
                fclose(command_file_ind);
                free(cmd_run);
                cmd_run = NULL;
                command_file_ind = NULL;
                continue;
            } 
            cmd_run[strlen(cmd_run) - 1] = '\0';
            print_command(cmd_run);
        } else if (command_n) {
            cmd_run = strdup(command_n);
            free(command_n);
            command_n = NULL;

        } else {
            ssize_t ret = getline(&cmd_run, &size_, stdin);
            flag_prompt = 1;
            if(ret == -1) {
                return 1;
            }
            cmd_run[strlen(cmd_run) - 1] = '\0';
        }
        if (strlen(cmd_run) < 1) { 
            free(cmd_run);
            cmd_run = NULL;
            continue;
        }
        vector_push_back(history_stor, cmd_run);
        vector_push_back(cmds_stor, cmd_run);
        int status = logical_operator_detector(cmd_run);
        free(cmd_run);
        cmd_run = NULL;
        if (status == 2) {
            break;
        }
    }


    if (history_file_ind != NULL) {
        size_t i = 0;
        size_t vec_size = vector_size(cmds_stor);
        for (i = 0; i < vec_size; i++) {
            fprintf(history_file_ind, "%s\n", ((char *) vector_get(cmds_stor, i)));
        }
        fclose(history_file_ind);
    }   

    vector_destroy(history_stor);
    vector_destroy(cmds_stor);
    
    size_t i = 0;
    size_t process_size = vector_size(vec_process);
    for(i = 0; i < process_size; i++) {
        process_t * curr = (process *)vector_get(vec_process, i);
        kill(curr -> pid, SIGKILL);
        destructor(curr -> pid);
    }
    vector_destroy(vec_process);

    if(command_file_ind) {
        fclose(command_file_ind);
    }
    free(curr_directory);
    
    return 0;
}