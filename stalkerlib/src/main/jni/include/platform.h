#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>

#define EVENT_ACCESS 1
#define EVENT_MODIFY 2
#define EVENT_MODIFY_NAME "IN_MODIFY"
#define EVENT_CREATE 4
#define EVENT_DELETE 8
#define TYPE_FILE 0
#define TYPE_DIR 1

#define TMP_FOLDER "stmp"

typedef int32_t event_type_t;

struct general_event_t {
	event_type_t eventType;
	char* filename;
	size_t len;
	int type;
} general_event_t;
typedef struct general_event_t event_t;

typedef void (*f_callback)(event_t*);

int startLib(int argc, char** argv);
int registerCallback(f_callback callback);
void closePlatform();

#endif