/**
 * nonstop_networking
 * CS 241 - Spring 2021
 */


#include <stdio.h>
#include "common.h"
#include "format.h"
#include "includes/dictionary.h"
#include "includes/vector.h"

#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>

#include <signal.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct client_requests {
    verb operation_key;
    int flag;
    char f[1000];
    size_t index;
} client_requests;

static dictionary* requests_dic;
static dictionary* file_size_dictionary;
static vector * curr_vec;
static char* storage_dir;
static size_t vec_len = 0;

static int epoll_file_des = 0;
static size_t pos_curr = 0;
static size_t byte_len = 1000;

void sigpipe_handler();
void close_sig_free();
void run_keycmd(int socket_des);

ssize_t read_sock_server(int num, size_t cnt, char *buffer) {
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
                fprintf(stderr, "Open read file fail\n");
                return -1;
            }
        } else if (ret == 0) {
            break;
        }
        index += ret;
    }
    return index;
}

ssize_t write_sock_server(int num, size_t cnt, char *buffer) {
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
                fprintf(stderr, "Open write file fail\n");
                return -1;
            }
        } else if (ret == 0) {
            break;
        }
        index += ret;
    }
    return index;
}

void r_w_file(int num, size_t cnt, FILE * file, int flag) {
    size_t visited = 0;
    if (flag == 1) {
        while (visited < cnt) {
            size_t curr_iter_size = 1024;
            if ((cnt - visited) < 1024) { 
                curr_iter_size = cnt - visited;
            } 
            char buffer[curr_iter_size + 1];
            buffer[curr_iter_size] = '\0';
            ssize_t curr_size = 0;
            curr_size = read_sock_server(num, curr_iter_size, buffer);
            if (curr_size == 0) {
                break;
            } else if (curr_size == -1) {
                continue;
            } else {
                visited += curr_size;
            }
            fwrite(buffer, 1, curr_size, file);
        }
        
    } else {
        while (visited < cnt) {
            size_t curr_iter_size = 1024;
            if ((cnt - visited) < 1024) { 
                curr_iter_size = cnt - visited;
            } 
            char buffer[curr_iter_size + 1];
            buffer[curr_iter_size] = '\0';
            ssize_t curr_size = 0;    
            fread(buffer, 1, curr_iter_size, file);
            curr_size = write_sock_server(num, curr_iter_size, buffer);
            if (curr_size == 0) {
                break;
            } else if (curr_size == -1) {
                continue;
            } else {
                visited += curr_size;
            }
        }
    }
}

void sigpipe_handler() {
}

void close_sig_free() {
    vector* curr = dictionary_values(requests_dic);
    VECTOR_FOR_EACH(curr, r, {
        free(r);
    });
    close(epoll_file_des);
    dictionary_destroy(requests_dic);
    dictionary_destroy(file_size_dictionary);
    vector_destroy(curr_vec);
    remove(storage_dir);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        print_server_usage();
        exit(1);
    }
    signal(SIGPIPE, sigpipe_handler);
    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(sig_act));
    sig_act.sa_handler = close_sig_free;
	if (sigaction(SIGINT, &sig_act, NULL) < 0) {
        perror("sigaction detected");
        exit(1);
    }
    requests_dic = int_to_shallow_dictionary_create();
    file_size_dictionary = string_to_unsigned_long_dictionary_create();
    curr_vec = string_vector_create();

    int file_des = socket(AF_INET, SOCK_STREAM, 0);
    int operation_key = 1;

    if (file_des == -1) {
        perror("socket error");
        exit(1);
    } else if (setsockopt(file_des, SOL_SOCKET,  SO_REUSEADDR, &operation_key, sizeof(operation_key)) == -1) {
        perror("setsocketopt error");
        exit(1);
    } 
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; 
    struct addrinfo *addrinfo_result;
    int add_resul = getaddrinfo(NULL, argv[1], &hints, &addrinfo_result);

    if (add_resul) {
        fprintf(stderr, "error, getaddrinfo: %s\n", gai_strerror(add_resul));
        exit(1);
    } else if (bind(file_des, addrinfo_result -> ai_addr, addrinfo_result -> ai_addrlen)) {
        perror("bind error");
        exit(1);
    } else if (listen(file_des, SOMAXCONN)) {
        perror("listen error");
        exit(1);
    }
    freeaddrinfo(addrinfo_result);

    char template[] = "XXXXXX";
    storage_dir = mkdtemp(template);
    print_temp_directory(storage_dir);
    
    epoll_file_des = epoll_create(1);
    struct epoll_event epoll_event;
    memset(&epoll_event, 0, sizeof(epoll_event));
    epoll_event.data.fd = file_des;
    epoll_event.events = EPOLLIN;
    epoll_ctl(epoll_file_des, EPOLL_CTL_ADD, file_des, &epoll_event);
    
    struct epoll_event events[10];
    while (1) {
        int triggerd_number = epoll_wait(epoll_file_des, events, 10, 1000);
        if (triggerd_number == -1) {
            exit(1);
        } 
        if (triggerd_number == 0) {
            continue;
        } else {
            for (int i = 0; i < triggerd_number; i++) {
                int socket_des = events[i].data.fd;
                if (events[i].data.fd == file_des) {
                    socket_des = accept(file_des, NULL, NULL);
                    client_requests* connect_request_reult = calloc(sizeof(client_requests), 1);
                    struct epoll_event epoll_event_client;
                    memset(&epoll_event_client, 0, sizeof(epoll_event_client));
                    epoll_event_client.data.fd = socket_des;
                    epoll_event_client.events = EPOLLIN;
                    epoll_ctl(epoll_file_des, EPOLL_CTL_ADD, socket_des, &epoll_event_client);
                    dictionary_set(requests_dic, &socket_des, connect_request_reult);
                    continue;
                }
                client_requests* connect_request_reult = dictionary_get(requests_dic, &socket_des);

                if (connect_request_reult -> flag == 1) {
                    run_keycmd(socket_des);

                } else if (connect_request_reult -> flag == 0) {
                    char buffer_read_res[byte_len];
                    memset(buffer_read_res, 0, byte_len);
                    size_t read_count = 0;
                    while (read_count < byte_len) {
                        char* curr_position = buffer_read_res + read_count;
                        //last
                        if (buffer_read_res[strlen(buffer_read_res) - 1] == '\n') {
                            break;
                        }
                        ssize_t r = read(socket_des, curr_position, 1);
                        if (r == -1) {
                            if (errno == EINTR) {
                                continue;
                            } 
                        } else {
                            if (r == 0) {
                                break;
                            }
                        }
                        read_count += r;
                    }
                    buffer_read_res[strlen(buffer_read_res) - 1] = '\0';

                    client_requests * dict_node = dictionary_get(requests_dic, &socket_des);
                    dict_node -> flag = 1;
                    if (strncmp("PUT", buffer_read_res, 3) == 0){
                        dict_node -> operation_key = PUT;
                        strcpy(dict_node -> f, buffer_read_res + 4);
                        pos_curr += 4;
                        char* file_path = NULL;
                        asprintf(&file_path, "%s/%s", storage_dir, dict_node -> f);
                        FILE* file = fopen(file_path, "w");
                        free(file_path);
                        if (!file) {
                            perror("Open file error");
                        } else {
                            size_t file_size;
                            read_sock_server(socket_des, sizeof(file_size), (char*) &file_size);
                            r_w_file(socket_des, file_size, file, 1);
                            dictionary_set(file_size_dictionary, dict_node -> f, &file_size);
                            vector_push_back(curr_vec, dict_node -> f);
                            vec_len += strlen(dict_node -> f) + 1;
                            fclose(file);
                        }
                        
                    } else if (strncmp("GET", buffer_read_res, 3) == 0) {
                        dict_node -> operation_key = GET;
                        pos_curr = pos_curr + 4;
                        strcpy(dict_node -> f, buffer_read_res + 4);

                    } else if (strncmp("DELETE", buffer_read_res, 6) == 0) {
                        dict_node -> operation_key = DELETE;
                        pos_curr = pos_curr + 7;
                        strcpy(dict_node -> f, buffer_read_res + 7);

                    } else if (strncmp("LIST", buffer_read_res, 4) == 0) {
                        pos_curr = pos_curr + 4;
                        dict_node -> operation_key = LIST;
                        
                    } else {
                        dict_node -> flag = 2;
                    }

                } else {
                    continue;
                }

            }
        }
    }
    shutdown(file_des, SHUT_RD);
    close(file_des);
}



void run_keycmd(int socket_des) {
    client_requests * dict_node = dictionary_get(requests_dic, &socket_des);
    if (dict_node -> operation_key == PUT){
        write_sock_server(socket_des, 3, "OK\n");
        free(dict_node);

    } else if (dict_node -> operation_key == GET) {
        char* directory = NULL;
        asprintf(&directory, "%s/%s", storage_dir, dict_node -> f);
        FILE * server_file = fopen(directory, "r");
        if (server_file) {
            void* curr_node = dictionary_get(file_size_dictionary, dict_node->f);
            size_t* ptr = (size_t*) curr_node;
            size_t file_size = *ptr;
            write_sock_server(socket_des, 3, "OK\n");
            write_sock_server(socket_des, sizeof(size_t), (char*) &file_size);
            r_w_file(socket_des, file_size, server_file, 0);
            free(directory);
            free(dict_node); 

        } else {
            write_sock_server(socket_des, 23, "Directory not existed\n");
            dict_node->flag = -1;
            free(directory);
            free(dict_node);
            return;   
        }
        

    } else if (dict_node -> operation_key == LIST) {
		write_sock_server(socket_des, 3, "OK\n");
		size_t curr_size = vec_len;
        if (curr_size > 0) {
            curr_size = curr_size - 1;
        }
		write_sock_server(socket_des, sizeof(size_t), (char*) &curr_size);
        size_t index = 0;
		VECTOR_FOR_EACH(curr_vec, f, {
            index++;
            write_sock_server(socket_des, strlen(f), f);
            if(_it < _iend - 1) {
                write_sock_server(socket_des, 1, "\n");
            } 
        });
        free(dict_node);

	} else if (dict_node -> operation_key == DELETE) {
		char* directory = NULL;
        asprintf(&directory, "%s/%s", storage_dir, dict_node -> f);
		remove(directory);
        // strlen(storage_dir) + 1;
		size_t target_index = 0;
        int flag_f = 0;
		VECTOR_FOR_EACH(curr_vec, f1, {
	        if (strcmp((char *) f1, dict_node->f) == 0){
               flag_f = 1;
	           break;
	        } else {
                target_index++;
            }
	    });
		if (flag_f == 0) {
            dict_node -> flag = -1;
            write_sock_server(socket_des, 15, "Unexisted target file\n");
		} else {
            vector_erase(curr_vec, target_index);
            dictionary_remove(file_size_dictionary, dict_node -> f);
            write_sock_server(socket_des, 3, "OK\n");
        }
        free(dict_node);
    } 
    epoll_ctl(epoll_file_des, EPOLL_CTL_DEL, socket_des, NULL);
    dictionary_remove(requests_dic, &socket_des);
    shutdown(socket_des, SHUT_RDWR);
    close(socket_des);
}