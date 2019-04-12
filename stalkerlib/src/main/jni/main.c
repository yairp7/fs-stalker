#include <stdlib.h>
#include <dlfcn.h>
#include "include/utils.h"

#define LIB_ANDROID "./libstalker.so"
#define LIB_LOGGING "./liblogging.so"

static struct logger_t* _logger = NULL;

struct logger_t {
	unsigned int size;
	void* handle;
	struct event_t* (*addEvent)(const long, const char*);
	void (*flush)();
} logger_t;

struct logger_t* initLogging() {
    if(fileExists(LIB_LOGGING) < 0) {
        perror("initLogging() - Can't find logging library.\n");
        return NULL;
    }

	struct logger_t* logger = (struct logger_t*)calloc(1, sizeof(struct logger_t));
	void* handle = dlopen(LIB_LOGGING, RTLD_NOW|RTLD_GLOBAL);
	if(handle) {
		logger->size = 0;
		logger->handle = handle;
		struct event_t* (*addEvent)(const long, const char*);
		void (*flush)();
		addEvent = (struct event_t* (*)(const long, const char*))dlsym(handle, "createEvent");
		flush = (void (*)())dlsym(handle, "flush");
		if(addEvent && flush) {
			logger->addEvent = addEvent;
			logger->flush = flush;
			_logger = logger;
			return _logger;
		}
		else {
			perror("initLogging() - Failed getting library symbols");
		}
	}
	else {
		perror("initLogging() - Failed opening library");
	}

	return NULL;
}

int main(int argc, char* argv[]) {
    if(fileExists(LIB_ANDROID) < 0) {
        perror("Can't find android library.\n");
        return 1;
    }

    initLogging();

    char currentUser[32];
    getCurrentUser(currentUser, 32);

    fprintf(stderr, "Stalker v1[Running as %s]\n", currentUser);

	void* handle = dlopen(LIB_ANDROID, RTLD_NOW|RTLD_GLOBAL);
	if(handle != NULL)
	{
		int (*startLib)(int, char**) = (int (*)(int, char**))dlsym(handle, "startLib");

		if(startLib) {
			if(startLib(argc, argv) == FAILED) {
				perror("module failed.\n"); 
				dlclose(handle);
				return 1;
			}
		}
		else {
			fprintf(stderr, "failed finding symbol - %s\n", dlerror()); 
			dlclose(handle);
			exit(1);
		}
	}
	else {
		perror("failed loading library.\n"); 
		exit(1);
	}
	dlclose(handle);

	return 0;
}