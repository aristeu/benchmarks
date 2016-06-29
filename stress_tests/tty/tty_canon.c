#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>

static const char *terminal = "/dev/console";
static const char *terminal2 = "/dev/ttyS0";

int open_term(const char *term)
{
	int i, fd, rc;

	close(0);
	close(1);
	close(2);

	for (i = 0; i < 3; i++) {
		fd = open(term, O_RDWR);
		if (fd < 0)
			return errno;
	}

	rc = ioctl(0, TIOCSCTTY, 1);
	if (rc) {
		perror("TIOCSCTTY");
		return errno;
	}

	return 0;
}

int term_setup(int fd)
{
	int rc;
	struct termios t;

	rc = tcgetattr(fd, &t);
	if (rc) {
		perror("tcgetattr");
		return errno;
	}

	t.c_cflag |= ICANON;

	rc = tcsetattr(fd, 0, &t);
	if (rc) {
		perror("tcsetattr");
		return errno;
	}
	return 0;
}

int new_session(const char *term)
{
	int rc;

	rc = setsid();
	if (rc < 0) {
		perror("Unable to create a new session");
		return errno;
	}

	return open_term(term);
}

int canon_process(const char *term)
{
	int rc;
	char buff[80];

	rc = new_session(term);
	if (rc)
		return rc;

	rc = term_setup(0);
	if (rc)
		return rc;

	while(1) {
		rc = read(0, buff, sizeof(buff));
		if (rc < 0) {
			rc = open_term(term);
			if (rc)
				return errno;
			rc = term_setup(0);
			if (rc)
				return rc;
		}
	}

	return 0;
}

int hangup_process(const char *term)
{
	int rc, fd;

	rc = new_session(term);
	if (rc)
		return rc;

	while(1) {
		usleep(100 * (rand() % 1000));
		vhangup();
		rc = open_term(term);
		if (rc)
			return rc;
	}

	return 0;
}

int write_process(const char *term)
{
	int rc;

	rc = new_session(term);
	if (rc)
		return rc;

	while(1) {
		usleep(100 * (rand() % 1000));
		rc = open_term(term);
		if (rc)
			return rc;
		printf(".\r\n");
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	pid_t pid[3];

	pid[0] = fork();
	if (pid[0] == 0) {
		return canon_process(terminal);
	} else if (pid < 0) {
		perror("fork");
		return 1;
	}

	pid[1] = fork();
	if (pid[1] == 0) {
		return hangup_process(terminal2);
	} else if (pid < 0) {
		perror("fork");
		kill(pid[0], SIGTERM);
		return 1;
	}

	pid[2] = fork();
	if (pid[2] == 0) {
		return write_process(terminal2);
	} else if (pid < 0) {
		perror("fork");
		kill(pid[0], SIGTERM);
		kill(pid[1], SIGTERM);
		return 1;
	}

	for (i = 0; i < sizeof(pid); i++)
		waitpid(pid[i], NULL, 0);

	return 0;
}

