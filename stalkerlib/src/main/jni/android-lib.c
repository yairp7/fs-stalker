#define _32

#include "include/platform.h"
#include "include/dir.h"
#include "include/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> // perror(), errno
#include <unistd.h> // read(), close(), getcwd()
#include <sys/inotify.h> // inotify_init(), inotify_add_watch()
#include <libgen.h> // basename()
#include <sys/types.h>
#include <sys/stat.h> // mkdir()

#define DEFAULT_WATCHS_CAPACITY 20

static f_callback _callbacks[4];
static int _callbacksSize = 0;
static int _inotifyFd = -1;
static int* _watchFds = NULL;
static int _watchFdsSize = 0;
static size_t _watchFdsCapacity = DEFAULT_WATCHS_CAPACITY;

#define MODE_COPY_MODIFIED "modified"

enum eCopyMode {
    NO_COPY,
    COPY_MODIFIED
};
typedef enum eCopyMode CopyMode;
static CopyMode _copyMode = NO_COPY;

static int _isRunning = 0;
static FILE* _outputFile = NULL;

void setOutputFile(char* outputFile) {
    _outputFile = fopen(outputFile, "a");
}

void setCopyMode(const char* arg) {
    if(arg != NULL) {
        if(strcmp(arg, MODE_COPY_MODIFIED) == 0) {
            _copyMode = COPY_MODIFIED;
            return;
        }
    }
    _copyMode = NO_COPY;
}

void readFileAndExit(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[1024];
    int c;
    while(fgets(line, sizeof(line), file) != NULL) {
        fputs (line, stdout);
    }
    fclose(file);
    exit(1);
}

int copyFile(const char* filePath, const char* eventType, const char* timestamp) {
    int pid = fork();
    if(pid == 0) {
        if(fileExists(TMP_FOLDER) == FAILED) {
            if(mkdir(TMP_FOLDER, S_IRWXU | S_IRWXG | S_IRWXO) == FAILED) {
                LOG(0, "Failed creating tmp folder!\n");
                exit(1);
            }
        }
        char targetDirPath[128];
        char targetPath[256];
        getcwd(targetDirPath, 128);
        sprintf(targetPath, "%s/%s/%s_%s_%s", targetDirPath, TMP_FOLDER, timestamp, eventType, basename(filePath));
        LOG(0, "Copying file %s to %s...\n", filePath, targetPath);
        FILE *sourceFile = fopen(filePath, "r");
        if (sourceFile) {
            FILE *targetFile = fopen(targetPath, "w");
            if (targetFile) {
                int ch;
                while ((ch = fgetc(sourceFile)) != EOF) {
                    fputc(ch, targetFile);
                }
                fclose(sourceFile);
                fclose(targetFile);
                exit(0);
            } else {
                LOG(0, "Target file can't be created!\n");
                fclose(sourceFile);
                exit(1);
            }
        }
        LOG(0, "Source file doesn't exist!\n");
        exit(1);
    }
    else {
        return SUCCESS;
    }
}

int checkArguments(int argc, char* argv[], int *pathIndex) {
    int hasOptions = 0;
    int hasPaths = 0;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-o") == 0) { // Arguments
            LOG(0, "Setting output file...\n");
            setOutputFile(argv[i + 1]);
            hasOptions = 1; i++; continue;
        }
        if(strcmp(argv[i], "-c") == 0) { // Arguments
            LOG(0, "Setting copy mode...\n");
            setCopyMode(argv[i + 1]);
            hasOptions = 1; i++; continue;
        }
        if(strcmp(argv[i], "-f") == 0) {
            LOG(0, "Reading file...\n");
            readFileAndExit(argv[i + 1]);
            hasOptions = 1; i++; continue;
        }
        else { // Paths
            *pathIndex = i;
            hasPaths = 1;
        }
    }
    if(hasPaths == 0 && hasOptions == 0) {
        return FAILED;
    }
    return SUCCESS;
}

int registerCallback(f_callback callback) {
    if(_callbacksSize < sizeof(_callbacks)) {
        _callbacks[_callbacksSize] = callback;
        return SUCCESS;
    }
    else {
        LOG(0, "Too many callbacks(max 4).\n");
    }
    return FAILED;
}

int addWatch(int inotifyFd, const char* pathToWatch) {
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE;
    if(_watchFds == NULL) {
        _watchFdsSize = 0;
        _watchFdsCapacity = DEFAULT_WATCHS_CAPACITY;
        _watchFds = (int *) calloc(_watchFdsCapacity, sizeof(int));
    }

    if(_watchFdsSize >= (int)(_watchFdsCapacity * 0.9)) {
        _watchFdsCapacity = _watchFdsCapacity * 2;
        int* watchFds = (int *) calloc(_watchFdsCapacity, sizeof(int));
        for(int i = 0; i < _watchFdsSize; i++) {
            watchFds[i] = _watchFds[i];
        }
        free(_watchFds);
        _watchFds = watchFds;
    }

    _watchFds[_watchFdsSize] = inotify_add_watch(inotifyFd, pathToWatch, mask);
    if (_watchFds[_watchFdsSize] >= 0) {
        _watchFdsSize++;
        return SUCCESS;
    }

    return FAILED;
}

int startLib(int argc, char **argv) {
    int pathIndex;
    if(checkArguments(argc, argv, &pathIndex) == FAILED) {
        LOG(0, "Bad syntax(Correct: fs-monitor [OPTIONS] <paths>).\n");
        return FAILED;
    }

    int pathc = argc - pathIndex;
    char** pathv = argv + pathIndex;

    LOG(0, "Starting android...\n");
    char eventsBuffer[4096];
    char eventMsg[512];
    struct dir_t* dir = NULL;
    int extraDirsSize = 0;

    if (pathc >= 1) {
        if(pathc == 1) { // Single directory
            dir = init(*pathv);
            extraDirsSize = dir != NULL ? dir->size : 0;
        }
    } else {
        fprintf(stderr, "No paths provided.\n");
        return FAILED;
    }

    _inotifyFd = inotify_init();
    if (_inotifyFd < 0) {
        fprintf(stderr, "inotify_init() failed!\n");
        return FAILED;
    }

    for (int i = 1, j = 1 - pathc; i < pathc || j < extraDirsSize; i++, j++) {
        char* pathAdded;
        int result;
        if(i < pathc) pathAdded = pathv[i];
        else if(j < extraDirsSize) pathAdded = dir->subDirs[j];
        result = addWatch(_inotifyFd, pathAdded);
        if(result == SUCCESS) LOG(0, "Added watch for \"%s\".\n", pathAdded);
        else fprintf(stderr, "inotify_add_watch failed.\n");
    }

    _isRunning = 1;

    while (_isRunning) {
        ssize_t eventsBufferSize = read(_inotifyFd, eventsBuffer, sizeof(eventsBuffer));
        if (eventsBufferSize < 0) {
            LOG(0, "failed reading.\n");
            continue;
        }

        char *buffPtr = eventsBuffer; // Beginning of the buffer
        char *buffPtrEnd = buffPtr + eventsBufferSize; // End of the events in the buffer

        while (buffPtr < buffPtrEnd) {
            event_t gEvent;
            struct inotify_event *event;
            size_t filePathLen = 0;

            // Check if we have enough space
            if ((buffPtrEnd - buffPtr) < sizeof(struct inotify_event)) {
                fprintf(stderr, "Not enough room for the inotify_event event, breaking.\n");
                break;
            }

            event = (struct inotify_event *) buffPtr;
            int c;
            char *eventMsgPtr = eventMsg;
            memset(eventMsg, 0, sizeof(eventMsg));
            char *currentPath;

            for (int i = 0, j = 1 - pathc; i < pathc || j < extraDirsSize; i++, j++) {
                if (_watchFds[i] == event->wd) {
                    if (i < pathc) {
                        currentPath = pathv[i];
                    }
                    else if(j < extraDirsSize) {
                        currentPath = dir->subDirs[j];
                    }
                    break;
                }
            }

            // struct extra_data_t *extra = (struct extra_data_t *) calloc(1, sizeof(struct extra_data_t));
            char *fullPath = (char *) calloc(strlen(currentPath) + event->len + 3, sizeof(char));
            char *fullPathPtr = fullPath;

            // Add the timestamp
            char timestamp[32] = {0};
            sprintf(timestamp, "%lld", getCurrentTimeMillis());

            // Add the directory path
            c = snprintf(fullPathPtr, strlen(currentPath) + 2, "%s/", currentPath);
            fullPathPtr += c;
            filePathLen += c;

            // Add the file name(is exists)
            if (event->len) {
                c = snprintf(fullPathPtr, event->len, "%s", event->name);
                filePathLen += c;
            }

            size_t msgLength = strlen(timestamp) + 2 + filePathLen;
            c = snprintf(eventMsgPtr, msgLength, "%s%c%s", timestamp, LOG_SEPERATOR, fullPath);
            eventMsgPtr += c;

            char eventType[10];
            // Add the event
            if (event->mask & IN_ACCESS) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_ACCESS");
                eventMsgPtr += c;
                gEvent.eventType = IN_ACCESS;
            }
            if (event->mask & IN_MODIFY) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, EVENT_MODIFY_NAME);
                eventMsgPtr += c;
                if(_copyMode == COPY_MODIFIED) {
                    copyFile(fullPath, EVENT_MODIFY_NAME, timestamp);
                }
                gEvent.eventType = IN_MODIFY;
            }
            if (event->mask & IN_CREATE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_CREATE");
                eventMsgPtr += c;
                gEvent.eventType = IN_CREATE;
            }
            if (event->mask & IN_DELETE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_DELETE");
                eventMsgPtr += c;
                gEvent.eventType = IN_DELETE;
            }

            *(eventMsgPtr++) = LOG_SEPERATOR;

            // Add if the event is related to a dir or file
            if (event->mask & IN_ISDIR) {
                c = snprintf(eventMsgPtr, 4, "DIR");
                eventMsgPtr += c;
            } else {
                c = snprintf(eventMsgPtr, 5, "FILE");
                eventMsgPtr += c;
            }

            // Check if a dir/file was created, and if so, watch it.
            if (event->mask & IN_CREATE) {
                addWatch(_inotifyFd, fullPath) == FAILED ?
                LOG(0, "inotify_add_watch failed.\n") : LOG(0, "Added watch for \"%s\".\n", fullPath);
            }

            gEvent.filename = fullPath;
            gEvent.len = strlen(fullPath) + 1;

            for(int i = 0; i < _callbacksSize; i++) {
                _callbacks[i](&gEvent);
            }

            free(fullPath);

            *(eventMsgPtr++) = '\n';

            LOG(0, "%sֿ", eventMsg);

            if(_outputFile != NULL) {
                LOG_FILE(_outputFile, eventMsg);
            }

            // Move to next event
            buffPtr += sizeof(struct inotify_event) + event->len;
        }
    }

    reset();

    closePlatform();

    return SUCCESS;
}

void closePlatform() {
    if(_outputFile != NULL) {
        fclose(_outputFile);
        _outputFile = NULL;
    }

    if(_watchFds != NULL) {
        for (int i = 0; i < _watchFdsSize; i++) {
            inotify_rm_watch(_inotifyFd, _watchFds[i]);
        }
        free(_watchFds);
        _watchFds = NULL;
    }

    if(_inotifyFd > 0) {
        close(_inotifyFd);
        _inotifyFd = -1;
    }
}