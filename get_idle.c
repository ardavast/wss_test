#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd;
	ssize_t len;
	unsigned long long data;
	int total = 0, ones = 0, zeros = 0;

	fd = open("/sys/kernel/mm/page_idle/bitmap", O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while ((len = read(fd, &data, 8) > 0)) {
		if (len < 0) {
			perror("read");
			exit(1);
		}

		total += 64;
		ones += __builtin_popcountll(data);
	}

	(void)close(fd);

	zeros = total - ones;

	/* convert to MiB */
	total /= 256;
	ones /= 256;
	zeros /= 256;

	(void)printf("total: %dMiB active: %dMiB idle: %dMiB\n", total, zeros, ones);

	return (0);
}
