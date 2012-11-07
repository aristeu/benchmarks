#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>

const char *sysfs = "/sys/class/net/";
#define MAX 50

int main(int argc, char *argv[])
{
	FILE *output;
	int fd, i;
	struct ifconf ifc;
	struct ifreq *ifr;
	char *buff;

	output = fopen("/tmp/output.txt", "w+");
	if (!output) {
		perror("Error opening file:");
		return 1;
	}

	fd = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(output, "Error creating socket: %s\n", strerror(errno));
		fclose(output);
		return 1;
	}
	
	buff = malloc(sizeof(*ifr) * MAX);
	if (!buff) {
		fprintf(output, "Not enough memory\n");
		fclose(output);
		return 1;
	}
	ifc.ifc_len = MAX * sizeof(*ifr);
	ifc.ifc_buf = buff;
	if (ioctl(fd, SIOCGIFCONF, &ifc)) {
		fprintf(output, "SIOCGIFCONF: %s\n", strerror(errno));
		fclose(output);
		return 1;
	}

	fprintf(output, "This is PID %i\n", getpid());
	for (i = 0; i < (ifc.ifc_len / sizeof(*ifr)); i++) {
		ifr = &ifc.ifc_req[i];
		fprintf(output, "%s\n", ifr->ifr_name);
	}

	close(fd);
	fclose(output);
}

