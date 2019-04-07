#define CONDITION_EQUALS 1
#define CONDITION_CONTAINS 2

#define ACTION_COPY 1
#define ACTION_DELETE 2

struct action_t {
	int eventType;
	int conditionFlags;
	char* conditionData;
	int actionFlags;
	char* actionData;
} action_t;

struct action_t* registerAction(int eventType, int conditionFlags, char* conditionData, int actionFlags, char* actionData);
int onEvent(struct general_event_t* event);
