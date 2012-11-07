#include <stdio.h>
#include <sched.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd;
	DIR *dir;
	char buff[50];
	struct dirent *dent;

	if (argc < 2) {
		fprintf(stderr, "%s <pid>\n", argv[0]);
		return 1;
	}
	snprintf(buff, sizeof(buff), "/proc/%s/ns", argv[1]);
	dir = opendir(buff);
	if (!dir) {
		perror("Error opening proc directory");
		return 1;
	}

	while(1) {
		dent = readdir(dir);
		if (!dent)
			break;

		/* skip '.' and '..' */
		if (*dent->d_name == '.')
			continue;

		snprintf(buff, sizeof(buff), "/proc/%s/ns/%s",
			 argv[1], dent->d_name);
		fd = open(buff, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Error opening ns file %s (%s)\n",
				dent->d_name, strerror(errno));
			continue;
		}
		if (setns(fd, 0)) {
			fprintf(stderr, "setns() error on %s (%s)\n",
				dent->d_name, strerror(errno));
			close(fd);
			continue;
		}
		printf("setns() to %s done sucessfully\n", dent->d_name);
		close(fd);
	}
	closedir(dir);
	return 0;
}

