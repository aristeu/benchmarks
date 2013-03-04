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
	[NETNS] = "netns",
	[PIDNS] = "pidns",
	[UTSNS] = "utsns",
	[USERNS] = "userns",
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
		if (fd < 0) {
			fprintf(stderr, "Unable to open %s, pid %i (%s)\n",
				buff, pid, strerror(errno));
			goto error;
		}
		data->nsfd[i] = fd;
		if (fstat(fd, &sbuff)) {
			fprintf(stderr, "Unable to stat %s (%s)\n", buff,
				strerror(errno));
			goto error;
		}
		data->inum[i] = sbuff.st_ino;
	}
	return 0;

error:
	i--;
	while (i-- >= 0) {
		close(data->nsfd[i]);
		data->nsfd[i] = -1;
	}
	return 1;
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

int get_next_different_process(struct setns_data *data, struct dirent **dent)
{
	struct dirent *retval, cur;
	struct setns_data d;
	DIR *dirp;
	int i, pid = -1;

	dirp = opendir("/proc");
	if (dirp == NULL)
		return -1;
	while (1) {
		if (readdir_r(dirp, &cur, &retval)) {
			perror("Error reading directory entries");
			closedir(dirp);
			return -1;
		}
		if (retval == NULL) {
			closedir(dirp);
			return -1;
		}
		pid = atoi(retval->d_name);
		if (pid == 0)
			continue;
		if (init_thread(&d, pid)) {
			closedir(dirp);
			return -1;
		}
		for (i = 0; i < NS_NUM; i++) {
			if (d.inum[i] != data->inum[i]) {
				*dent = retval;
				cleanup_thread(&d);
				return pid;
			}
		}
		cleanup_thread(&d);
	}
	closedir(dirp);

	return -1;
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
			if (unshare(u))
				return errno;
		} else {
			if (wait(&rc) == -1)
				return errno;
			return rc;
		}
	} else {
		if (unshare(u))
			return errno;
	}
	return 0;
}

#define STRESSER_UNIT(n, f, priv) { .d.number = n, .fn = f, .fnpriv = (void *)priv, }
static void *unshare_test(struct stresser_config *config)
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
		return config;
	return NULL;
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
		if (unshare(u))
			return errno;
		/* sleep for at least 5s */
		sleep((rand() % 10) + 5);
	} else {
		if (wait(&rc) == -1)
			return errno;
		return rc;
	}
	return 0;
}

static int _setns_test(void *data)
{
	struct setns_data data;
	rc = init_thread(&data, getpid());

}

static void *setns_test(struct stresser_config *config)
{
	struct setns_data data;
	struct stresser_unit set[] = {
				      STRESSER_UNIT(50, _setns_random_unshare, NULL),
				      STRESSER_UNIT(100, _setns_test, NULL),
				     };

}

const char *options = "sh";
int main(int argc, char *argv[])
{
	struct stresser_config cfg;

	cfg.distribution = STRESSD_FIXED;
	cfg.stress_type = STRESST_HORDE;

	unshare_test(&cfg);

	return 0;
}

