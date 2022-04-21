/**
 * nonstop_networking
 * CS 241 - Spring 2021
 */
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"

char **parse_args(int argc, char **argv);
verb check_args(char **args);

ssize_t read_sock(int num, size_t cnt, char *buffer) {
    size_t index = 0;
    while (1) {
        if (index >= cnt) {
            break;
        }
        ssize_t ret = read(num, buffer + index, cnt - index);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        } else if (ret == 0) {
            break;
        }
        index += ret;
    }
    return index;
}

ssize_t write_sock(int num, size_t cnt, const char *buffer) {
    size_t index = 0;
    while (1) {
        if (index >= cnt) {
            break;
        }
      ssize_t ret = write(num, buffer + index, cnt - index);
      if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        } else if (ret == 0) {
            break;
        }
        index += ret;
    }
    return index;
    
}

int connector(char *host, char *port) {
    struct addrinfo hints;
    struct addrinfo *hints_ptr = &hints;
    memset(hints_ptr, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    struct addrinfo *curr;
    int ret = getaddrinfo(host, port, &hints, &curr);
    int sock_file = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (sock_file == -1) {
        perror("socket failed");
        exit(1);
    } else {
        if (ret != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
            exit(1);
        } else {
            ret = connect(sock_file, curr->ai_addr, curr->ai_addrlen);
            if (ret == -1) {
                perror("connect failed");
                exit(1);
            }
        }
    }
    freeaddrinfo(curr);
    return sock_file;
}

void list_key(char **arguments, int num) {
    char *method = arguments[2];
    char *to_print = malloc(2 + strlen(method));
    sprintf(to_print, "%s\n", method);
    ssize_t result = write_sock(num, strlen(to_print), to_print);
    if (result < (ssize_t)strlen(to_print)) {
        print_connection_closed();
        exit(1);
    }
    free(to_print);
}

void other_key(char **arguments, int num) {
    char *method = arguments[2];
    char *to_print = malloc(3 + strlen(method) + strlen(arguments[3]));
    sprintf(to_print, "%s %s\n", method, arguments[3]);
    ssize_t result = write_sock(num, strlen(to_print), to_print);
    if (result < (ssize_t)strlen(to_print)) {
        print_connection_closed();
        exit(1);
    }
    free(to_print);
}

void put_key(char **arguments, int num) {
    char *method = arguments[2];
    char *file_dest = arguments[4];
    struct stat state;
    if(stat(file_dest, &state) == -1) {
        exit(1);
    }
    char *to_print = malloc(3 + strlen(method) + strlen(arguments[3]));
    sprintf(to_print, "%s %s\n", method, arguments[3]);
    ssize_t result = write_sock(num, strlen(to_print), to_print);
    if (result < (ssize_t)strlen(to_print)) {
        print_connection_closed();
        exit(1);
    }
    size_t st_size = state.st_size;
    write_sock(num, sizeof(size_t), (char*) &st_size);

    FILE *f = fopen(file_dest, "r");
    if (f == NULL) {
        exit(1);
    }
    size_t write_size = 0;
    ssize_t read_size = 1024;
    while (write_size < st_size) {
        read_size = 1024;
        if ((st_size - write_size) <= 1024) { 
            read_size = st_size - write_size;
        } 
        char buffer[read_size + 1];
        ssize_t ret_fread = fread(buffer, 1, read_size, f);
        if (ret_fread != read_size) {
            exit(-1);
        }
        ssize_t result = write_sock(num, read_size, buffer);
        if (result < read_size) {
            print_connection_closed();
            exit(1);
        }
        write_size += read_size;
    }
    fclose(f);
    free(to_print);
}


int main(int argc, char **argv) {
    char **arguments = parse_args(argc, argv);
    verb key_word = check_args(arguments); 
    char * host = arguments[0];
    char* port = arguments[1];

    int socket_connector = connector(host, port);
    if (key_word == PUT) {
        int fd = open(arguments[4], O_RDONLY);
        if (fd == -1) { 
            perror("Directory not existed");
            close(fd);
            exit(1);
        } else {
            put_key(arguments, socket_connector);
        }
    } else if (key_word == LIST){
        list_key(arguments, socket_connector);
    } else {
        other_key(arguments, socket_connector);
    }

    if (shutdown(socket_connector, SHUT_WR) != 0) {
        perror("shutdown()");
    }

    char *ok_string = "OK\n";
    size_t ok_len = strlen(ok_string);
    char *buffer = calloc(1, ok_len + 1);

    char *error_string = "ERROR\n";
    size_t error_len = strlen(error_string);
    char *buffer2 = calloc(1, error_len + 1);
    char *file_dest = arguments[4];
    size_t ret_sock = read_sock(socket_connector, ok_len, buffer);

    if (strcmp(buffer, ok_string) != 0) {
        fprintf(stdout, "%s", buffer2);
        read_sock(socket_connector, error_len - ret_sock, buffer2 + ret_sock);
        char error_list[5];
        if (strcmp(buffer2, error_string) != 0) {
            print_invalid_response();
        } else {
            ret_sock = read_sock(socket_connector, 5, error_list);
            if (ret_sock == 0) {
                print_connection_closed();
            }
        }
        free(buffer);
        free(buffer2);

        shutdown(socket_connector, SHUT_RD);
        close(socket_connector);
        free(arguments);

    } else {
        fprintf(stdout, "%s", buffer);

        if (key_word == PUT) {
            print_success();

        } else if (key_word == GET) {
            FILE *f = fopen(file_dest, "a+");
            if (!f) {
                perror(NULL);
                exit(1);
            }
            size_t st_size = 0;
            read_sock(socket_connector, sizeof(size_t), (char *)&st_size);
            size_t st_size_use = st_size + 5;
            size_t curr_filled = 0;
            while (1) {
                if (curr_filled >= st_size_use) {
                    break;
                }
                char get_buff[1025];
                size_t read_size = 1024;
                if ((st_size_use - curr_filled) <= 1024) {
                    read_size = st_size_use - curr_filled;
                }
                size_t connect_ret = read_sock(socket_connector, read_size, get_buff);
                fwrite(get_buff, 1, connect_ret, f);
                
                if (connect_ret == 0) {
                    break;
                }
                curr_filled = curr_filled + connect_ret;
            }

            if (curr_filled == 0 && curr_filled != st_size) {
                print_connection_closed();
                exit(1);
            }

            if (curr_filled != st_size)  {
                if (curr_filled < st_size) {
                    print_too_little_data();
                    exit(1);
                } else if (curr_filled > st_size) {
                    print_received_too_much_data();
                    exit(1);
                }
            }

            fclose(f);

        } else if (key_word == LIST) {
            size_t st_size = 0;
            read_sock(socket_connector, sizeof(size_t), (char *)&st_size);
            int exit_flag = 0;
            char * list_buff = calloc(1, st_size + 1);
            ret_sock = read_sock(socket_connector, st_size, list_buff);
            printf("%s\n", list_buff);
            free(list_buff);

            if (ret_sock == 0 && ret_sock != st_size) {
                print_connection_closed();
                exit_flag = 1;
                exit(exit_flag);
            } else if (ret_sock < st_size) {
                print_too_little_data();
                exit_flag = 1;
                exit(exit_flag);
            } else if (ret_sock > st_size) {
                print_received_too_much_data();
                exit_flag = 1;
                exit(exit_flag);
            }

        } else if (key_word == DELETE) {
            print_success();
        }
        free(buffer);
        free(buffer2);
        shutdown(socket_connector, SHUT_RD);
        close(socket_connector);
        free(arguments);
    }
}


/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }
    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }
    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;

    } else if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);

    } else if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);

    } else if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}