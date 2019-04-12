#define EVENT_ACCESS 1
#define EVENT_MODIFY 2
#define EVENT_CREATE 4
#define EVENT_DELETE 8
#define TYPE_FILE 0
#define TYPE_DIR 1

static int isRunning = 0;

struct general_event_t {
	int eventType;
	char* filename;
	int len;
	int type;
} general_event_t;

int startLib(int argc, char** argv);
struct general_event_t* generateEvent(void* originalEvent, void* extraData);
void closePlatform();