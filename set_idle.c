#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	int fd;
	ssize_t len;
	unsigned long long data = 0xffffffffffffffffULL;
	int total = 0;

	fd = open("/sys/kernel/mm/page_idle/bitmap", O_WRONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	while ((len = write(fd, &data, 8)) > 0) {
		if (len < 0) {
			perror("write");
			exit(1);
		}

		total += len * 8;
	}

	(void)close(fd);

	/* convert to MiB */
	total /= 256;

	(void)printf("marked as idle: %dMiB\n", total);

	return (0);
}
