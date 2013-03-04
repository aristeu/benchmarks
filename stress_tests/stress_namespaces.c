#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#define __USE_GNU 1
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "stresser.h"

#define NS_NUM 6
enum nsid {
	IPC = 0,
	MNTNS,
	NETNS,
	PIDNS,
	USERNS,
	UTSNS,
};

#ifndef __NR_setns
# ifdef __i386__
#define __NR_setns		346
# else
#define __NR_setns		308
# endif
#define setns(a, b) syscall(__NR_setns, a, b)
#endif


const char *nsnames[] = {
	[IPC] = "ipc",
	[MNTNS] = "mnt",
	[NETNS] = "netns",
	[PIDNS] = "pidns",
	[USERNS] = "userns",
	[UTSNS] = "utsns",
};

struct data {
	int nsfd[NS_NUM];
	ino_t inum[NS_NUM];
};

int init_thread(struct data *data, int pid)
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

void cleanup_thread(struct data *data)
{
	int i;

	for (i = 0; i < NS_NUM; i++)
		close(data->nsfd[i]);
}

int change_ns(struct data *data, enum nsid id, int fd)
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

int get_next_different_process(struct data *data, struct dirent **dent)
{
	struct dirent *retval, cur;
	struct data d;
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


static int _setns_stress_thread(void *data)
{
	printf("one\n");
	return 0;
}

static int _setns_stress_thread2(void *data)
{
	printf("two\n");
	return 0;
}
static int _setns_stress_thread3(void *data)
{
	printf("three\n");
	return 0;
}

static int run_setns_stress(struct stresser_config *config)
{
	struct stresser_unit set[] = {
				      { .d.chance = 2,
					.fn = _setns_stress_thread, },
				      { .d.chance = 5,
					.fn = _setns_stress_thread2, },
				      { .d.chance = 3,
					.fn = _setns_stress_thread3, },
				     };

	return stresser(config, set, 3, NULL);
}

const char *options = "sh";
int main(int argc, char *argv[])
{
	struct stresser_config cfg;

	cfg.threads = 10;
	cfg.distribution = STRESSD_RANDOM;
	cfg.stress_type = STRESST_HORDE;

	run_setns_stress(&cfg);
	return 0;
}

