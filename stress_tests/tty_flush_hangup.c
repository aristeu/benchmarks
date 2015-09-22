/*
 * tty TCFLSH/hangup stress test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "stresser.h"

static int flush(void *priv)
{
	int fd, rc;

again:
	fd  = open((char *)priv, O_RDWR);
	if (fd < 0) {
		perror("open");
		goto again;
	}
	rc = ioctl(fd, TIOCSCTTY, 1);
	if (rc) {
		perror("TIOCSCTTY");
		close(fd);
		goto again;
	}

	while(1) {
		if (tcflush(fd, TCIFLUSH)) {
			close(fd);
			goto again;
		}
	}
	return 0;
}

static int hangup(void *priv)
{
	while(1) {
		sleep(random() % 2);
		if (vhangup()) {
			continue;
		}
	}
	return 0;
}

#if 0
static int tty_write(void *priv)
{
	int fd, rc;

	fd  = open((char *)priv, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	rc = ioctl(fd, TIOCSCTTY, 1);
	if (rc) {
		perror("TIOCSCTTY");
		return 1;
	}

	while(1) {
		write(fd, "abc", 3);
		sleep(random() % 3);
	}
	return 0;
}
#endif

static struct stresser_config config = {
	.stress_type = STRESST_HORDE,
	.distribution = STRESSD_FIXED,
};

static struct stresser_unit units[] = {
	{ .d.number = 200, .fn = hangup, },
	{ .d.number = 200, .fn = flush, },
};

int main(int argc, char *argv[])
{
	int fd, rc;
	pid_t pid;

	if (argc < 2) {
		fprintf(stderr, "%s <tty device>\n", argv[0]);
		return 1;
	}

	pid = fork();
	if (pid > 0) {
		waitpid(pid, NULL, 0);
		return 0;
	}
	signal(SIGHUP, SIG_IGN);

	fd  = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (setsid() < 0) {
		perror("setsid");
		return 1;
	}

	/* controlling tty */
	rc = ioctl(fd, TIOCSCTTY, 1);
	if (rc) {
		perror("TIOCSCTTY");
		return 1;
	}

	if (stresser(&config, units, 2, argv[1]))
		perror("stresser");

	return 0;
}

