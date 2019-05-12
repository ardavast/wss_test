#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#define IDLE_BITMAP_PATH "/sys/kernel/mm/page_idle/bitmap"
#define KPAGEFLAGS_PATH "/proc/kpageflags"
#define MEMWASTE_SIZE (1024 * 1024 * 1024) /* 1 GiB */

static void errx(const char *s);
static int xopen(const char *pathname, int flags);
static void xclose(int fd);
static void *xmalloc(size_t size);
static void xsleep(double seconds);
static int get_npages(void);
static void drop_pagecache(void);
static void set_idle(void);
static void read_idle(void);
static int is_page_active(uint64_t pageflags);
static int get_nactive(void);
static void pr_page_stats(int npages, int nactive);
static pid_t memwaste_start(void);
static void memwaste_stop(pid_t pid);

static void errx(const char *s)
{
	perror(s);
	exit(1);
}

static int xopen(const char *pathname, int flags)
{
	int fd;

	fd = open(pathname, flags);
	if (fd < 0) {
		errx("open");
	}

	return (fd);
}

static void xclose(int fd)
{
	int ret;

	ret = close(fd);
	if (ret) {
		errx("close");
	}
}

static void *xmalloc(size_t size)
{
	void *buf;

	buf = malloc(size);
	if (buf == NULL) {
		errx("malloc");
	}

	return (buf);
}

static void xsleep(double seconds)
{
	int ret;

	ret = usleep((unsigned int)(seconds * 1e6));
	if (ret < 0) {
		errx("usleep");
	}
}

static int get_npages(void)
{
	int fd;
	uint64_t *buf;
	size_t bufsize = 1024 * sizeof(uint64_t);
	ssize_t len;
	int count = 0;

	fd = xopen(KPAGEFLAGS_PATH, O_RDONLY);
	buf = xmalloc(bufsize);

	while ((len = read(fd, buf, bufsize)) > 0) {
		if (len < 0) {
			errx("read");
		}

		count += len / sizeof(uint64_t);
	}

	free(buf);
	xclose(fd);

	return (count);
}

static void drop_pagecache(void)
{
	int fd;
	char buf = '3';
	ssize_t len;

	sync();

	fd = xopen("/proc/sys/vm/drop_caches", O_WRONLY);

	len = write(fd, &buf, sizeof(buf));
	if (len != sizeof(buf)) {
		errx("write");
	}
}

static void set_idle(void)
{
	int fd;
	uint64_t buf = 0xffffffffffffffffULL;
	ssize_t len;

	fd = xopen(IDLE_BITMAP_PATH, O_WRONLY);

	while ((len = write(fd, &buf, sizeof(buf))) > 0) {
		if (len < 0) {
			errx("write");
		}
	}

	xclose(fd);
}

static void read_idle(void)
{
	int fd;
	uint64_t buf;
	ssize_t len;

	fd = xopen(IDLE_BITMAP_PATH, O_RDONLY);

	while ((len = read(fd, &buf, sizeof(buf))) > 0) {
		if (len < 0) {
			errx("read");
		}
	}

	xclose(fd);
}

static int is_page_active(uint64_t pageflags)
{
#define FLAGS_ULA  0x0000000000000068ULL /* uptodate | lru | active */
#define FLAGS_IDLE 0x0000000002000000ULL /* idle */

	if ((pageflags & FLAGS_ULA) != FLAGS_ULA) {
		return (0);
	}

	if ((pageflags & FLAGS_IDLE)) {
		return (0);
	}

	return (1);
}

static int get_nactive(void)
{
	int fd;
	uint64_t *buf;
	size_t bufsize = 1024 * sizeof(uint64_t);
	ssize_t len;
	int count = 0;

	fd = xopen(KPAGEFLAGS_PATH, O_RDONLY);
	buf = xmalloc(bufsize);

	while ((len = read(fd, buf, bufsize)) > 0) {
		if (len < 0) {
			errx("read");
		}

		for (size_t i = 0; i < (len / sizeof(uint64_t)); i++) {
			if (is_page_active(buf[i])) {
				count++;
			}
		}
	}

	free(buf);
	xclose(fd);

	return(count);
}

static void pr_page_stats(int npages, int nactive)
{
	static int pagesize;
	int nidle;

	if (pagesize == 0) {
		pagesize = sysconf(_SC_PAGE_SIZE);
		if (pagesize == -1) {
			errx("sysconf");
		}
	}

	nidle = npages - nactive;

#define p2m(x) (((long)(x) * pagesize) / 1048576)

	(void)printf("active/idle/total: %ld/%ld/%ldMiB (%d/%d/%d pages)\n",
	  p2m(nactive), p2m(nidle), p2m(npages),
	  nactive, nidle, npages);
}

static pid_t memwaste_start(void)
{
	pid_t pid;
	unsigned char *buf;

	pid = fork();
	if (pid == -1) {
		errx("fork");
	} else if (pid > 0) {
		return (pid);
	}

	srand(time(NULL));
	buf = malloc(MEMWASTE_SIZE);

	for (;;) {
		unsigned char byte = rand() % 256;
		unsigned long long sum;

		/* write */
		memset(buf, byte, MEMWASTE_SIZE);

		/* read */
		sum = 0;
		for (int i = 0; i < MEMWASTE_SIZE; i++) {
			sum += buf[i];
		}

		xsleep(1000);
	}

	return (0);
}

static void memwaste_stop(pid_t pid)
{
	int ret;
	pid_t retpid;

	ret = kill(pid, SIGKILL);
	if (ret) {
		errx("kill");
	}

	retpid = waitpid(pid, NULL, 0);
	if (retpid != pid) {
		errx("waitpid");
	}
}

int main(void)
{
	int npages;
	pid_t mw_pid1, mw_pid2;

	npages = get_npages();

	(void)printf("init ");
	pr_page_stats(npages, get_nactive());

	(void)puts("dropping the page cache");
	drop_pagecache();
	xsleep(1);

	(void)puts("setting / reading idle bitmap");
	set_idle();
	xsleep(1);
	read_idle();
	xsleep(1);

	for (int i = 1; i <= 100; i++) {
		switch (i) {
		case 10:
			(void)puts("allocate 1GB");
			mw_pid1 = memwaste_start();
			break;
		case 30:
			(void)puts("allocate 1GB");
			mw_pid2 = memwaste_start();
			break;
		case 50:
			(void)puts("free 1GB");
			memwaste_stop(mw_pid1);
			break;
		case 70:
			(void)puts("free 1GB");
			memwaste_stop(mw_pid2);
			break;
		}

		(void)printf("#%03d ", i);
		pr_page_stats(npages, get_nactive());

		xsleep(0.5);
	}
}
