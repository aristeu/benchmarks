#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
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

/* */
struct stresser_config {
	/** maximum number of threads to run, stresser_unit.number will be
	    ignored if set */
	unsigned int threads;
	/** stress type determines the way threads are created and started */
	enum {
		STRESS_HORDE,	/* threads start all at once */
		STRESS_FIFO,	/* threads start as they're created */
	} stress_type;

	/** pick the unit based on chance of n of 10 passed in
	    stresser_unit.chance, ignores stresser_unit.number.
	    Must have 2 or more units to use chance */
	bool use_chance;
};

struct stresser_unit {
	union {
		unsigned int number;	/* number of threads */
		unsigned char chance;	/* unit chance in 10 of running */
	};
	/** if stresser_config.threads is set and stresser_unit.chance are not
	    set, determines the proportion 1-100 of this unit's threads. this
	    allows creating tests that 1/4 of the threads are x, 2/4 are y and
	    the another 1/4 are z, no matter the total number of threads */
	unsigned char proportion;
	int (*fn)(void *priv);
};
struct stresser_priv {
	void *fnpriv;		/* function private data */
	int fnrc;		/* function retval */
	bool wait;		/* conditional wait? */
	pthread_cond_t *cond;	/* conditional wait */
	pthread_mutex_t *mutex;	/* conditional wait mutex */
	struct stresser_unit *u /* stresser unit */
};

static int _stresser_unit(void *p)
{
	struct stresser_priv *priv = p;
	int rc;

	if (p->wait) {
		rc = pthread_cond_wait(p->cond, p->mutex);
		if (rc)
			return rc;
	}

	p->fnrc = p->u->fn(p->fnpriv);

	return 0;
}

static int stresser(struct stress_config *cfg, struct stresser_unit *set,
		    int n_unit, void *fnpriv)
{
	struct stresser_priv *priv;
	pthread_cont_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int threads, i;

	/* sanity checks */
	if (cfg->use_chance && n_unit < 2)
		return EINVAL;
	if (n_unit == 0)
		return EINVAL;

	/* first determine the total number of threads */
	if (cfg->threads)
		threads = cfg->threads;
	else {
		threads = 0;
		for (i = 0; i < n_unit; i++)
			threads += set[i].number;
		if (threads == 0)
			return EINVAL;
	}

	/* allocating private structures */
	priv = calloc(sizeof(*priv), threads);
	if (priv == NULL)
		return ENOMEM;


	for (i = 0; i < threads; i++) {
		priv[i].fnpriv = fnpriv;

		/* prepare the conditional wait if needed */
		if (cfg->stress_type == STRESS_HORDE) {
			priv[i].cond = &cond;
			priv[i].mutex = &mutex;
			priv[i].wait = true;
		}

		/* if units have chance to happen */
		if (cfg->use_chance == true) {
			int u, highest = 0;
			for (u = 0; u < n_unit; u++) {
				if (!(rand() % set[u].chance)) {
					highest = u;
					break;
				}
				if (set[u].chance > set[highest].chance)
					highest = u;
			}
			priv[i].unit = &set[highest];
		/* fixed number of unit runs */
		} else if (!cfg->threads) {
			int u;
			for (u = 0; u < n_unit; u++)
				if (set[u].number)
					break;
			if (u == n_unit) {
				fprintf(stderr, "Internal error: %s:%i\n",
					__FILE__, __LINE__);
				exit(1);
			}
			priv[i].unit = &set[u];
			set[u].number--;
		/* proportional unit runs */
		} else {

		}
	}
}
/* */

static int _setns_stress_thread(void *data)
{

}

static int run_setns_stress(struct config *config)
{
	stressfn set[] = {{__setns_stress_thread, 10}, {NULL,}};

	return stresser(config, __setns_stress_thread, NULL);
}

const char *options = "sh";
int main(int argc, char *argv[])
{
	struct stress_config cfg;

	cfg.threads = 1000;
	cfg.stress_type = STRESS_HORDE;

	run_setns_stress(&cfg);
	return 0;
}

