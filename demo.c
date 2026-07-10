#include <stdio.h>
#include "cpnbi.h"

int
main() {
	int e = 0;

	cpnbi_init();
	while (1) {
		int32_t e = cpnbi_get_event();
		printf("Event: %d Key: %c Mod: %d Type: %d Is Special: %d\n", e, cpnbi_event_key(e), cpnbi_event_mod(e), cpnbi_event_type(e), cpnbi_event_is_special(e));
	}

	return 0;
}
