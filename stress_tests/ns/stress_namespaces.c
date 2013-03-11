#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "stresser.h"

#ifndef __NR_setns
# ifdef __i386__
#define __NR_setns		346
# else
#define __NR_setns		308
# endif
#define setns(a, b) syscall(__NR_setns, a, b)
#endif

#ifndef CLONE_NEWNS
#define CLONE_NEWNS	0x00020000
#endif

#ifndef CLONE_NEWIPC
#define CLONE_NEWIPC	0x08000000
#endif

#ifndef CLONE_NEWNET
#define CLONE_NEWNET	0x40000000
#endif

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS	0x04000000
#endif

#ifndef CLONE_NEWPID
#define CLONE_NEWPID	0x20000000
#endif

#ifdef HAS_USERNS
#ifndef CLONE_NEWUSER
#define CLONE_NEWUSER	0x10000000
#endif

#define NS_NUM 6

#define ALL_NAMESPACES (CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWNET|CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWUSER)

#else	/* !HAS_USERNS */

#define NS_NUM 5

#define ALL_NAMESPACES (CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWNET|CLONE_NEWUTS|CLONE_NEWPID)

#endif	/* HAS_USERNS */

enum nsid {
	IPC = 0,
	MNTNS,
	NETNS,
	PIDNS,
	UTSNS,
	USERNS,
};

const char *nsnames[] = {
	[IPC] = "ipc",
	[MNTNS] = "mnt",
	[NETNS] = "net",
	[PIDNS] = "pid",
	[UTSNS] = "uts",
	[USERNS] = "user",
};

const int nscloneflags[] = {
	[IPC] = CLONE_NEWIPC,
	[MNTNS] = CLONE_NEWNS,
	[NETNS] = CLONE_NEWNET,
	[PIDNS] = CLONE_NEWPID,
	[UTSNS] = CLONE_NEWUTS,
	[USERNS] = CLONE_NEWUSER,
};

struct setns_data {
	int nsfd[NS_NUM];
	ino_t inum[NS_NUM];
};

int init_thread(struct setns_data *data, int pid)
{
	int i, fd;
	char buff[50];
	struct stat sbuff;

	for (i = 0; i < NS_NUM; i++) {
		snprintf(buff, sizeof(buff), "/proc/%i/ns/%s", pid, nsnames[i]);
		fd = open(buff, O_RDONLY);
		if (fd < 0)
			goto error;
		data->nsfd[i] = fd;
		if (fstat(fd, &sbuff))
			goto error;
		data->inum[i] = sbuff.st_ino;
	}
	return 0;

error:
	i--;
	while (i-- >= 0) {
		close(data->nsfd[i]);
		data->nsfd[i] = -1;
	}
	return errno;
}

void cleanup_thread(struct setns_data *data)
{
	int i;

	for (i = 0; i < NS_NUM; i++)
		close(data->nsfd[i]);
}

int change_ns(struct setns_data *data, enum nsid id, int fd)
{
	struct stat sbuff;

	if (fstat(fd, &sbuff)) {
		perror ("change_ns: unable to stat new fd");
		return 1;
	}
	if (setns(fd, 0)) {
		fprintf(stderr, "change_ns: error changing namespace %s (%s)\n",
			nsnames[id], strerror(errno));
		return 1;
	}
	close(data->nsfd[id]);
	data->nsfd[id] = fd;
	data->inum[id] = sbuff.st_ino;

	return 0;

}

static int pid_dir_exists(int pid)
{
	struct stat s;
	char buff[50];

	sprintf(buff, "/proc/%i/ns", pid);

	return !stat(buff, &s);
}

int get_next_different_process(struct setns_data *data, struct setns_data *output)
{
	struct dirent *retval, cur;
	DIR *dirp;
	int i, pid = -1;

	dirp = opendir("/proc");
	if (dirp == NULL)
		return -1;
	while (1) {
		if ((errno = readdir_r(dirp, &cur, &retval))) {
			perror("Error reading directory entries");
			closedir(dirp);
			return -1;
		}
		if (retval == NULL)
			break;

		pid = atoi(retval->d_name);
		if (pid == 0)
			continue;
		if (init_thread(output, pid)) {
			/* the process might just have disappeared, check */
			int e = errno;
			if (pid_dir_exists(pid)) {
				closedir(dirp);
				errno = e;
				return -1;
			}
			continue;
		}
		for (i = 0; i < NS_NUM; i++) {
			if (output->inum[i] != data->inum[i]) {
				closedir(dirp);
				return i;
			}
		}
		cleanup_thread(output);
	}
	closedir(dirp);

	return 0;
}

static int _unshare_test(void *data)
{
	long u = (long)data;

	/* unshare with CLONE_NEWPID requires a new process */
	if (u & CLONE_NEWPID) {
		int pid = fork(), rc;

		if (pid == -1)
			return errno;

		if (pid == 0) {
			int pid2;

			if (unshare(u))
				return errno;
			/* it'll be only active if a new process is created */
			pid2 = fork();
			if (pid2 == 0)
				exit(123);
			else {
				if (wait(&rc) == -1)
					exit(errno);
				if (WEXITSTATUS(rc) != 123)
					exit(1);
			}
			exit (0);
		} else {
			if (wait(&rc) == -1)
				return errno;
			return WEXITSTATUS(rc);
		}
	} else {
		if (unshare(u))
			return errno;
	}
	return 0;
}

#define STRESSER_UNIT(n, f, priv) { .d.number = n, .fn = f, .fnpriv = (void *)priv, }
static int unshare_test(struct stresser_config *config)
{
	struct stresser_unit set[] = {
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWNS),
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWIPC),
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWNET),
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWUTS),
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWPID),
#ifdef HAS_USERNS
				      STRESSER_UNIT(50, _unshare_test, CLONE_NEWUSER),
#endif
				      STRESSER_UNIT(50, _unshare_test, ALL_NAMESPACES),
				     };
	int rc;

	rc = stresser(config, set, (sizeof(set) / sizeof(struct stresser_unit)), NULL);
	if (rc)
		return rc;
	return 0;
}

static int _setns_random_unshare(void *data)
{
	int r, pid, rc;

	srand(time(NULL));
	r = rand() % NS_NUM;

	pid = fork();

	if (pid == -1)
		return errno;

	if (pid == 0) {
		if (unshare(r))
			return errno;
		/* sleep for at least 5s */
		sleep((rand() % 10) + 5);
	} else {
		if (wait(&rc) == -1)
			return errno;
		return WEXITSTATUS(rc);
	}
	return 0;
}

static int _setns_test(void *unused)
{
	struct setns_data data, other;
	int i, rc, pid, max = 20;

	pid = fork();
	if (pid < 0)
		return errno;

	if (pid == 0) {
		rc = init_thread(&data, getpid());
		if (rc)
			exit(rc);

		while (max--) {
			i = get_next_different_process(&data, &other);
			if (i < 0)
				exit(errno);
			if (i == 0)
				break;
			rc = change_ns(&data, i, other.nsfd[i]);
			if (rc)
				exit(errno);
		}
		exit(0);
	}
	if (wait(&rc) == -1)
		return errno;
	return WEXITSTATUS(rc);
}

static int setns_test(struct stresser_config *config)
{
	struct stresser_unit set1[] = {
				      STRESSER_UNIT(50, _setns_random_unshare, NULL),
				     };
	struct stresser_unit set2[] = {
				      STRESSER_UNIT(100, _setns_test, NULL),
				     };
	int pid, rc, rc2;

	pid = fork();
	if (pid == -1)
		return errno;
	if (pid == 0) {
		exit(stresser(config, set1, (sizeof(set1) / sizeof(struct stresser_unit)), NULL));
	} else {
		/* we just give the other units a bit of time */
		sleep(2);
		rc = stresser(config, set2, (sizeof(set2) / sizeof(struct stresser_unit)), NULL);
		if (wait(&rc2) == -1)
			return errno;
		if (rc2)
			return WEXITSTATUS(rc2);
		if (rc)
			return rc;
	}

	return 0;
}

const char *options = "sh";
int main(int argc, char *argv[])
{
	struct stresser_config cfg;
	int rc;

	cfg.distribution = STRESSD_FIXED;
	cfg.stress_type = STRESST_HORDE;

	rc = unshare_test(&cfg);
	printf("unshare test %s\n", rc? "FAILED":"passed");
	if (rc)
		return rc;
	rc = setns_test(&cfg);
	printf("setns test %s\n", rc? "FAILED":"passed");

	return rc;
}

