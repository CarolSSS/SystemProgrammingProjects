/**
 * nonstop_networking
 * CS 241 - Spring 2021
 */
#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "format.h"


#define LOG(...)                      \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (0);

typedef enum { GET, PUT, DELETE, LIST, V_UNKNOWN } verb;

