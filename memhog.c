#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ALLOC_SIZE (1024 * 1024 * 1024) /* 1 GiB */

int main(void)
{
	unsigned char *buf;
	unsigned char byte;
	unsigned long long p;
	int i;

	srand(time(NULL));
	buf = malloc(ALLOC_SIZE);

	for (;;) {
		byte = rand() % 256;
		(void)printf("fill buffer with value: 0x%02x... ", byte);
		memset(buf, byte, ALLOC_SIZE);

		p = 0;
		for (i = 0; i < ALLOC_SIZE; i++) {
			p += buf[i];
		}
		(void)printf("read buffer sum: 0x%016llx\n", p);

		(void)sleep(1);
	}

	return (0);
}
