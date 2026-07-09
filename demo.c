#include <stdio.h>
#include "cpnbi.h"

int
main() {
	int e = 0;

	cpnbi_init();
	while (1) {
		printf("Event: %d\n", cpnbi_get_event());
	}

	return 0;
}
