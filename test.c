#include "cpnbi.h"
#include<stdio.h>

int main() {
	int e = 0;

	cpnbi_init();
	while (1) {
		while (!cpnbi_is_event_available()) {}
		printf("Event: %d\n", cpnbi_get_event());
		/*
		e = cpnbi_get_event();
		printf("Event: %d\n", e);
		*/
	}
	cpnbi_shutdown();

	return 0;
}
