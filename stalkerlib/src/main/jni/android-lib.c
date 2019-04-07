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

int startLib(int argc, char **argv) {
    LOG("Starting android...", 0, 0);
    int inotifyFd;
    int *watchFds;
    char eventsBuffer[4096];
    char eventMsg[512];
    struct dir_t* dir = NULL;
    int extraDirsSize = 0;

    if (argc > 1) {
        if(argc == 2) { // Single directory
            dir = init(argv[1]);
            extraDirsSize = dir != NULL ? dir->size : 0;
        }
        watchFds = (int *) calloc(argc + extraDirsSize - 1, sizeof(int));
    } else {
        perror("No paths provided.\n");
        return FAILED;
    }

    inotifyFd = inotify_init();
    if (inotifyFd < 0) {
        perror("inotify_init() failed!\n");
        switch (errno) {
            case EMFILE:
                perror("-> The user limit on the total number of inotify instances has been reached.\n");
                perror("-> The per-process limit on the number of open file descriptors has been reached.\n");
                break;
            case ENFILE:
                perror("-> The system-wide limit on the total number of open files has been reached.\n");
                break;
            case ENOMEM:
                perror("-> Insufficient kernel memory is available.\n");
                break;
        }
        return FAILED;
    }

    for (int i = 1, j = 1 - argc; i < argc || j < extraDirsSize; i++, j++) {
        char* pathAdded;
        if(i < argc) {
            pathAdded = argv[i];
            watchFds[i - 1] = inotify_add_watch(inotifyFd, pathAdded, IN_ACCESS | IN_MODIFY | IN_CREATE | IN_DELETE);
        }
        else if(j < extraDirsSize) {
            pathAdded = dir->subDirs[j];
            watchFds[i - 1] = inotify_add_watch(inotifyFd, pathAdded, IN_ACCESS | IN_MODIFY | IN_CREATE | IN_DELETE);
        }

        if (watchFds < 0) {
            perror("inotify_add_watch failed.\n");
            switch (errno) {
                case EACCES:
                    perror("-> Read access to the given file is not permitted.\n");
                    break;

                case EBADF:
                    perror("-> The given file descriptor is not valid.\n");
                    break;

                case EFAULT:
                    perror("-> pathname points outside of the process's accessible address space.\n");
                    break;

                case EINVAL:
                    perror("-> The given event mask contains no valid events; or mask contains both IN_MASK_ADD and IN_MASK_CREATE; or fd is not an inotify file descriptor.\n");
                    break;

                case ENAMETOOLONG:
                    perror("-> pathname is too long.\n");
                    break;

                case ENOENT:
                    perror("-> A directory component in pathname does not exist or is a dangling symbolic link.\n");
                    break;

                case ENOMEM:
                    perror("-> Insufficient kernel memory was available.\n");
                    break;

                case ENOSPC:
                    perror("-> The user limit on the total number of inotify watches was reached or the kernel failed to allocate a needed resource.\n");
                    break;

                case ENOTDIR:
                    perror("-> mask contains IN_ONLYDIR and pathname is not a directory.\n");
                    break;

                case EEXIST:
                    perror("-> mask contains IN_MASK_CREATE and pathname refers to a file already being watched by the same fd.\n");
                    break;
            }
            return FAILED;
        }
        fprintf(stderr, "Added watch for \"%s\".\n", pathAdded);
    }

    isRunning = 1;

    while (isRunning) {
        ssize_t eventsBufferSize = read(inotifyFd, eventsBuffer, sizeof(eventsBuffer));
        if (eventsBufferSize < 0) {
            perror("failed reading.\n");
            continue;
        }

        char *buffPtr = eventsBuffer; // Beginning of the buffer
        char *buffPtrEnd = buffPtr + eventsBufferSize; // End of the events in the buffer

        while (buffPtr < buffPtrEnd) {
            struct inotify_event *event;
            int filePathLen;

            // Check if we have enough space
            if ((buffPtrEnd - buffPtr) < sizeof(struct inotify_event)) {
                perror("Not enough room for the inotify_event event, breaking.\n");
                break;
            }

            event = (struct inotify_event *) buffPtr;
            int c;
            char *eventMsgPtr = eventMsg;
            memset(eventMsg, 0, sizeof(eventMsg));
            char *currentPath;

            for (int i = 0, j = 1 - argc; i < argc || j < extraDirsSize; i++, j++) {
                if (watchFds[i] == event->wd) {
                    if (i < argc) {
                        currentPath = argv[i + 1];
                    }
                    else if(j < extraDirsSize) {
                        currentPath = dir->subDirs[j];
                    }
                    break;
                }
            }

            struct extra_data_t *extra = (struct extra_data_t *) calloc(1, sizeof(struct extra_data_t));
            char *fullPath = (char *) calloc(strlen(currentPath) + event->len + 2, sizeof(char));
            char *fullPathPtr = fullPath;

            // Add the directory path
            c = snprintf(fullPathPtr, strlen(currentPath) + 2, "%s/", currentPath);
            fullPathPtr += c;
            filePathLen += c;

            // Add the file name(is exists)
            if (event->len) {
                c = snprintf(fullPathPtr, event->len - 1, "%s", event->name);
                fullPathPtr += c;
                filePathLen += c;
            }

            c = snprintf(eventMsgPtr, filePathLen, "%s", fullPath);
            eventMsgPtr += c;

            // extra->data = fullPathPtr;
            // extra->len = filePathLen;

            // Add the event
            if (event->mask & IN_ACCESS) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = '|';
                c = snprintf(eventMsgPtr, 10, "IN_ACCESS");
                eventMsgPtr += c;
            }
            if (event->mask & IN_MODIFY) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = '|';
                c = snprintf(eventMsgPtr, 10, "IN_MODIFY");
                eventMsgPtr += c;
            }
            if (event->mask & IN_CREATE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = '|';
                c = snprintf(eventMsgPtr, 10, "IN_CREATE");
                eventMsgPtr += c;
            }
            if (event->mask & IN_DELETE) {
                if ((eventMsgPtr - eventMsg) > 0) *(eventMsgPtr++) = '|';
                c = snprintf(eventMsgPtr, 10, "IN_DELETE");
                eventMsgPtr += c;
            }

            *(eventMsgPtr++) = '|';

            // Add if the event is related to a dir or file
            if (event->mask & IN_ISDIR) {
                c = snprintf(eventMsgPtr, 4, "DIR");
                eventMsgPtr += c;
            } else {
                c = snprintf(eventMsgPtr, 5, "FILE");
                eventMsgPtr += c;
            }

            *(eventMsgPtr++) = '\0';

            LOG(eventMsg, 0, 0);

            // struct general_event_t *gEvent = generateEvent(event, extra);
            // free(extra->data);
            // free(extra);

            // Move to next event
            buffPtr += sizeof(struct inotify_event) + event->len;
        }
    }

    reset();
    for (int i = 0; i < (argc - 1); i++) {
        inotify_rm_watch(inotifyFd, watchFds[i]);
    }
    free(watchFds);
    close(inotifyFd);

    return SUCCESS;
}
