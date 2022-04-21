/**
 * vector
 * CS 241 - Spring 2021
 */
#include "sstring.h"
#include "vector.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>

struct sstring {
    // Anything you want
    vector * vec;
};

sstring *cstr_to_sstring(const char *input) {
    // your code goes here
    sstring * ret = malloc(sizeof(sstring));
    ret->vec = char_vector_create();
    const char* input_ir = input;
    while (*input_ir) {
        vector_push_back(ret->vec, (void*)input_ir);
        input_ir++;
    }
    return ret;
}

char *sstring_to_cstr(sstring *input) {
    size_t i = 0;
    char * ret = malloc((vector_size(input->vec) + 1) * sizeof(char));
    for(i = 0; i < vector_size(input->vec); i++) {
        ret[i] = *(char*)vector_get(input->vec,i);
        // ret[i] = input->vec[i];
    }
    ret[vector_size(input->vec)] = '\0';
    return ret;
}

int sstring_append(sstring *this, sstring *addition) {
    // your code goes here
    // sstring * ret = malloc(sizeof(sstring));
    sstring *addition1 = addition;
    size_t i = 0;
    for(i = 0; i < vector_size(addition->vec); i++) {
        vector_push_back(this->vec,vector_get(addition1->vec,i));
    }
    return vector_size(this->vec);
}

vector *sstring_split(sstring *this, char delimiter) {
    // your code goes here
    // this->vec ;
    vector * ret = string_vector_create();
    size_t i = 0;
    // size_t cnt = 1;
    // for(i = 0; i < vector_size(this->vec); i++) {
    //     if (!strcmp(vector_get(this->vec,i),delimiter)) {
    //         cnt++;
    //     }
    // }

    size_t cnt = 0;
    char* buffer = malloc(vector_size(this->vec) * sizeof(char));
    for(i = 0; i < vector_size(this->vec); i++) {
        if (*((char*)vector_get(this->vec,i)) != delimiter) {
            // printf("char is %s\n",(char*)vector_get(this->vec,i) );
            buffer[cnt] = *((char*)vector_get(this->vec,i));
            // printf("whole buffer is %s\n",buffer);
            cnt++;
        } else {
            // printf("delimeter is %s\n",(char*)vector_get(this->vec,i) );
            buffer[cnt] = '\0';
            //printf("inputing whole buffer is %s\n",buffer);

            vector_push_back(ret, buffer);
            cnt = 0;
        } 
        if (i == vector_size(this->vec) - 1) {
            buffer[cnt] = '\0';
            vector_push_back(ret, buffer);
        }
    }
    free(buffer);
    // printf("ret 0 %s\n",ret);
    return ret;
}

int sstring_substitute(sstring *this, size_t offset, char *target,
                       char *substitution) {
    size_t i = 0;
    size_t j = 0;
    int flag_appear = 0;
    for ( i = offset; i < vector_size(this->vec); i++) {
        if (*((char*)vector_get(this->vec,i)) == *target) {
            int still_flag = 1;
            for ( j = 1; j < strlen(target); j++) {
                if (*((char*)vector_get(this->vec,i + j)) != target[j]) {
                    still_flag = 0;
                    break;
                }
            }
            if (still_flag == 0) {
                // i = i + j -1; 
                continue;
            } else {
                flag_appear = 1;
                for ( j = 0; j < strlen(target); j++) {
                    vector_erase(this->vec, i);
                }
                for ( j = 0; j < strlen(substitution); j++) {
                    vector_insert(this->vec, (i +j), substitution + j);
                }
                // i = i + j -1; 
            }
            break;
        } 
    }

    if (flag_appear == 0) {
        return -1;
    } else {
        return 0;
    }
    return 0;
    
}

char *sstring_slice(sstring *this, int start, int end) {
    char * ret = (char*) malloc((end - start + 1) * sizeof(char));
    int i = 0;
    for(i = start; i < end; i++) {
        ret[i - start] = *((char*)vector_get(this->vec,i));
    }
    ret[(end - start)]= '\0'; 
    return ret;
}

void sstring_destroy(sstring *this) {
    vector_destroy(this->vec);
    free(this);
}
