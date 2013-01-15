#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#define NS_NUM 6
enum nsid {
	IPC = 0,
	MNTNS,
	NETNS,
	PIDNS,
	USERNS,
	UTSNS,
};

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
			fprintf(stderr, "Unable to stat %s (%s)\n", buffer,
				strerror(errno));
			goto error;
		}
		data->inum[i] = sbuff.st_ino;
	}
	return 0;

error:
	i--;
	while (i-- >= 0) {
		close(nsfd[i]);
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
	close(data->nsfd[id]);
	if (setns(fd, 0)) {
		fprintf(stderr, "change_ns: error changing namespace %s (%s)\n",
			nsnames[id], strerror(errno));
		return 1;
	}
	data->nsfd[id] = fd;
	data->inum[id] = sbuff.st_ino;

	return 0;

}

int get_next_different_process(struct data *data, struct dirent **dent)
{
	int i, pid;
	struct dirent *retval;
	struct data d;

	while (1) {
		if (readdir_r(dirp, *dent, &retval)) {
			perror("Error reading directory entries");
			return -1;
		}
		if (retval == NULL)
			return -1;
		pid = atoi(retval->d_name);
		if (pid == 0)
			continue;
		if (init_thread(&data, pid))
			return -1;
		for (i = 0; i < NS_NUM; i++) {
			if (d->inum[i] != data->inum[i])
				break;
	}
	*dent = retval;
	cleanup_thread(&d);

	return pid;
}

int main(int argc, char *argv[])
{
	return 0;
}

