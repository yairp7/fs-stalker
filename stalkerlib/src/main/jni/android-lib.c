#define _32

#include "include/platform.h"
#include "include/dir.h"
#include "include/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> // perror(), errno
#include <unistd.h> // read(), close()
#include <sys/inotify.h> // inotify_init(), inotify_add_watch()

static FILE* _outputFile = NULL;
static int _inotifyFd = -1;
static int* _watchFds = NULL;
static int _watchFdsCount = 0;

struct extra_data_t { 
	int len;
	char* data;
} extra_data_t;

struct general_event_t *generateEvent(void *originalEvent, void *extraData) {
    struct inotify_event *oEvent = (struct inotify_event *) originalEvent;
    struct general_event_t *cEvent = (struct general_event_t *) calloc(1, sizeof(struct general_event_t));
    struct extra_data_t *extra = extraData;
    if (oEvent->mask & IN_ACCESS) {
        cEvent->eventType |= EVENT_ACCESS;
    }
    if (oEvent->mask & IN_MODIFY) {
        cEvent->eventType |= EVENT_MODIFY;
    }
    if (oEvent->mask & IN_CREATE) {
        cEvent->eventType |= EVENT_CREATE;
    }
    if (oEvent->mask & IN_DELETE) {
        cEvent->eventType |= EVENT_DELETE;
    }
    if (oEvent->mask & IN_ISDIR) {
        cEvent->type = TYPE_DIR;
    } else {
        cEvent->type = TYPE_FILE;
    }
    if (oEvent->len) {
        cEvent->len = extra->len;
        cEvent->filename = (char *)calloc(extra->len + 1, sizeof(char));
        memcpy(cEvent->filename, extra->data, extra->len);
    }
    return cEvent;
}

void setOutputFile(char* outputFile) {
    _outputFile = fopen(outputFile, "a");
}

int checkArguments(int argc, char* argv[], int *pathIndex) {
    int hasOptions = 0;
    int hasPaths = 0;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-o") == 0) { // Arguments
            LOG(0, "Setting output file...\n");
            setOutputFile(argv[i + 1]);
            hasOptions = 1;
            i++;
        }
        else { // Paths
            *pathIndex = i;
            hasPaths = 1;
        }
    }
    if(hasPaths == 0) {
        return FAILED;
    }
    return SUCCESS;
}

int startLib(int argc, char **argv) {
    int pathIndex;
    if(checkArguments(argc, argv, &pathIndex) == FAILED) {
        LOG(0, "Bad syntax(Correct: fs-stalker [OPTIONS] <paths>).\n");
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
        int size = pathc + extraDirsSize - 1;
        _watchFds = (int *) calloc(size, sizeof(int));
        _watchFdsCount = size;
    } else {
        fprintf(stderr, "No paths provided.\n");
        return FAILED;
    }

    _inotifyFd = inotify_init();
    if (_inotifyFd < 0) {
        fprintf(stderr, "inotify_init() failed!\n");
        switch (errno) {
            case EMFILE:
                fprintf(stderr, "-> The user limit on the total number of inotify instances has been reached.\n");
                fprintf(stderr, "-> The per-process limit on the number of open file descriptors has been reached.\n");
                break;
            case ENFILE:
                fprintf(stderr, "-> The system-wide limit on the total number of open files has been reached.\n");
                break;
            case ENOMEM:
                fprintf(stderr, "-> Insufficient kernel memory is available.\n");
                break;
        }
        return FAILED;
    }

    for (int i = 1, j = 1 - pathc; i < pathc || j < extraDirsSize; i++, j++) {
        char* pathAdded;
        if(i < pathc) {
            pathAdded = pathv[i];
            _watchFds[i - 1] = inotify_add_watch(_inotifyFd, pathAdded, IN_ACCESS | IN_MODIFY | IN_CREATE | IN_DELETE);
        }
        else if(j < extraDirsSize) {
            pathAdded = dir->subDirs[j];
            _watchFds[i - 1] = inotify_add_watch(_inotifyFd, pathAdded, IN_ACCESS | IN_MODIFY | IN_CREATE | IN_DELETE);
        }

        if (_watchFds < 0) {
            fprintf(stderr, "inotify_add_watch failed.\n");
            switch (errno) {
                case EACCES:
                    fprintf(stderr, "-> Read access to the given file is not permitted.\n");
                break;

                case EBADF:
                    fprintf(stderr, "-> The given file descriptor is not valid.\n");
                break;

                case EFAULT:
                    fprintf(stderr, "-> pathname points outside of the process's accessible address space.\n");
                break;

                case EINVAL:
                    fprintf(stderr, "-> The given event mask contains no valid events; or mask contains both IN_MASK_ADD and IN_MASK_CREATE; or fd is not an inotify file descriptor.\n");
                break;

                case ENAMETOOLONG:
                    fprintf(stderr, "-> pathname is too long.\n");
                break;

                case ENOENT:
                    fprintf(stderr, "-> A directory component in pathname does not exist or is a dangling symbolic link.\n");
                break;

                case ENOMEM:
                    fprintf(stderr, "-> Insufficient kernel memory was available.\n");
                break;

                case ENOSPC:
                    fprintf(stderr, "-> The user limit on the total number of inotify watches was reached or the kernel failed to allocate a needed resource.\n");
                break;

                case ENOTDIR:
                    fprintf(stderr, "-> mask contains IN_ONLYDIR and pathname is not a directory.\n");
                break;

                case EEXIST:
                    fprintf(stderr, "-> mask contains IN_MASK_CREATE and pathname refers to a file already being watched by the same fd.\n");
                break;
            }
            return FAILED;
        }
        LOG(0, "Added watch for \"%s\".\n", pathAdded);
    }

    isRunning = 1;

    while (isRunning) {
        ssize_t eventsBufferSize = read(_inotifyFd, eventsBuffer, sizeof(eventsBuffer));
        if (eventsBufferSize < 0) {
            LOG(0, "failed reading.\n");
            continue;
        }

        char *buffPtr = eventsBuffer; // Beginning of the buffer
        char *buffPtrEnd = buffPtr + eventsBufferSize; // End of the events in the buffer

        while (buffPtr < buffPtrEnd) {
            struct inotify_event *event;
            int filePathLen;

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
            char *fullPath = (char *) calloc(strlen(currentPath) + event->len + 2, sizeof(char));
            char *fullPathPtr = fullPath;

            // Add the timestamp
            char timestamp[32] = {0};
            c = sprintf(timestamp, "%lld", getCurrentTimeMillis());
            char* timestampPtr = timestamp + c;
            *(timestampPtr++) = LOG_SEPERATOR;

            c = snprintf(eventMsgPtr, strlen(timestamp) + 2, "%s", timestamp);
            eventMsgPtr += c;

            // Add the directory path
            c = snprintf(fullPathPtr, strlen(currentPath) + 2, "%s/", currentPath);
            fullPathPtr += c;
            filePathLen += c;

            // Add the file name(is exists)
            if (event->len) {
                c = snprintf(fullPathPtr, event->len, "%s", event->name);
                fullPathPtr += c;
                filePathLen += c;
            }

            c = snprintf(eventMsgPtr, filePathLen, "%s", fullPath);
            eventMsgPtr += c;

            free(fullPath);

            // extra->data = fullPathPtr;
            // extra->len = filePathLen;

            // Add the event
            if (event->mask & IN_ACCESS) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_ACCESS");
                eventMsgPtr += c;
            }
            if (event->mask & IN_MODIFY) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_MODIFY");
                eventMsgPtr += c;
            }
            if (event->mask & IN_CREATE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_CREATE");
                eventMsgPtr += c;
            }
            if (event->mask & IN_DELETE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = LOG_SEPERATOR;
                c = snprintf(eventMsgPtr, 10, "IN_DELETE");
                eventMsgPtr += c;
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

            *(eventMsgPtr++) = '\n';

            LOG(0, "%sֿ", eventMsg);

            if(_outputFile != NULL) {
                LOG_FILE(_outputFile, eventMsg);
            }

            // struct general_event_t *gEvent = generateEvent(event, extra);
            // free(extra->data);
            // free(extra);

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
        for (int i = 0; i < _watchFdsCount; i++) {
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