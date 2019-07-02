//
// Created by Yair Pecherer on 05/04/2019.
//

#ifndef STALKER_DIR_H
#define STALKER_DIR_H

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define DEFAULT_NUM_OF_DIRS 10

struct dir_t {
    char** subDirs;
    int size;
    int capacity;
} dir_t;

struct dir_t* init(char* rootPath);
struct dir_t* get();
void reset();

#endif
