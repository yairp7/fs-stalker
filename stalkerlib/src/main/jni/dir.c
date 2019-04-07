//
// Created by Yair Pecherer on 05/04/2019.
//

#include "dir.h"

void reset() {
    if(current != NULL) {
        for(int i = 0; i < current->size; i++) {
            free(current->subDirs[i]);
        }
        free(current->subDirs);
        free(current);
    }
}

struct dir_t* init(char* rootPath) {
    if(current != NULL) {
        reset();
    }

    current = (struct dir_t*)calloc(1, sizeof(struct dir_t));
    current->size = 0;
    current->capacity = DEFAULT_NUM_OF_DIRS;
    current->subDirs = (char**)calloc(current->capacity, sizeof(char*));

    listDir(rootPath, addDir);

    return current;
}

static void addDir(char* dirPath) {
    if(current != NULL) {
        if(current->size >= current->capacity * 0.8) {
            int newCapacity = current->capacity * 2;
            char** newSubDirs = (char**)calloc(newCapacity, sizeof(char*));
            current->capacity = newCapacity;
            for(int i = 0; i < current->size; i++) {
                newSubDirs[i] = current->subDirs[i];
            }
            free(current->subDirs);
            current->subDirs = newSubDirs;
        }

        char* dirPathCopy = (char*)calloc(strlen(dirPath) + 1, sizeof(char));
        strcpy(dirPathCopy, dirPath);
        current->subDirs[current->size] = dirPathCopy;
        current->size++;
    }
}

struct dir_t* get() {
    return current;
}

static void listDir(const char* root, void (*addDir)(char* dirPath)) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(root))) return;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
            addDir(path);
            listDir(path, addDir);
        }
    }
    closedir(dir);
}