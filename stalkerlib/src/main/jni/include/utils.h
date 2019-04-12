#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <math.h>
#include <time.h> // timestamp
#include <unistd.h> // access()
#include <string.h>
#include <stdarg.h>

#define SUCCESS 0
#define FAILED -1

#define LOG_SEPERATOR '|'
#define LOG_PREFIX_FORMAT "[+]"

void LOG(const int level, const char *format, ...);
void LOG_FILE(FILE* file, char* msg);
long long getCurrentTimeMillis();
int fileExists(char* filename);
void getCurrentUser(char *buf, size_t size);

#endif