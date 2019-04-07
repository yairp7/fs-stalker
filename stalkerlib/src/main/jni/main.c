#include <stdlib.h>
#include <dlfcn.h>
#include "include/utils.h"

#define LIB_ANDROID "./libstalker.so"

int main(int argc, char* argv[]) {
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