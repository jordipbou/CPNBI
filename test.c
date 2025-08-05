#include "cpnbi.h"
#include<stdio.h>

int main() {
	cpnbi_init();
	while (1) {
		while (!cpnbi_is_event_available()) {}
		printf("Event: %d\n", cpnbi_get_event());
	}
	cpnbi_shutdown();

	return 0;
}
