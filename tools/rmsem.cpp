#include <semaphore.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		int ret = sem_unlink(argv[i]);
		if (ret)
			fprintf(stderr, "Failed to unlink: %s\n", argv[i]);
	}

	return 0;
}
