//
// Created by Yair Pecherer on 04/04/2019.
//
#include "include/utils.h"

void LOG(const int level, const char *format, ...) {
    int formatLength = strlen(format);
    char* formatPrefix = LOG_PREFIX_FORMAT;
    int finalFormatLength = formatLength + strlen(formatPrefix) + 2;
    char finalFormat[finalFormatLength];
    snprintf(finalFormat, finalFormatLength, "%s %s", formatPrefix, format);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, finalFormat, args);
    va_end(args);
}

void LOG_FILE(FILE* file, char* msg) {
    fprintf(file, "%s", msg);
}

long long getCurrentTimeMillis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000LL + tv.tv_usec/1000;
}

int fileExists(char* filename) {
    if(access(filename, F_OK) != -1) {
        return 0;
    }
    return -1;
}

void getCurrentUser(char *buf, size_t size) {
    char* user = getlogin();
    memcpy(buf, user, size);
}