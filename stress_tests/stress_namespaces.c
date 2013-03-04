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
		STRESST_HORDE,	/* threads start all at once */
		STRESST_FIFO,	/* threads start as they're created */
	} stress_type;

	enum {
		/* threads are created randomly, based on the units' chance */
		STRESSD_RANDOM,
		/* each unit has a fixed number of threads created, 'threads'
		 * is ignored */
		STRESSD_FIXED,
		/* proportional: 1-100 proportion for each unit. make sure the
		 * sum of all units is 100 */
		STRESSD_PROP,
	} distribution;
};

struct stresser_unit {
	union {
		unsigned int number;		/* fixed number of threads */
		unsigned char chance;		/* unit chance 1-10 of running */
		unsigned char proportion;	/* 1-100 proportion this unit will run */
	} d;
	int (*fn)(void *priv);
	/* internal only, will be modified */
	unsigned int _threads;
};
struct stresser_priv {
	void *fnpriv;		/* function private data */
	int fnrc;		/* function retval */
	int wait;		/* conditional wait? */
	pthread_cond_t *cond;	/* conditional wait */
	pthread_mutex_t *mutex;	/* conditional wait mutex */
	pthread_t thread;	/* thread identifier */
	struct stresser_unit *u;/* stresser unit */
	int *count;		/* number of initialized threads */
};

static void *_stresser_unit(void *p)
{
	struct stresser_priv *priv = p;
	int rc;

	if (priv->wait) {
		pthread_mutex_lock(priv->mutex);
		*priv->count = *priv->count + 1;
		rc = pthread_cond_wait(priv->cond, priv->mutex);
		pthread_mutex_unlock(priv->mutex);
		if (rc)
			return (void *)1;
	}

	priv->fnrc = priv->u->fn(priv->fnpriv);

	return NULL;
}

static int stresser(struct stresser_config *cfg, struct stresser_unit *set,
		    int n_unit, void *fnpriv)
{
	struct stresser_priv *priv;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int threads = 0, i, u, highest, rc = 0, count = 0, tries;

	/* sanity checks */
	if (n_unit == 0)
		return EINVAL;

	/* first determine the total number of threads */
	switch (cfg->distribution) {
	case STRESSD_RANDOM:
		srand(time(NULL));
		threads = cfg->threads;
		break;
	case STRESSD_FIXED:
		threads = 0;
		for (i = 0; i < n_unit; i++) {
			threads += set[i].d.number;
			set[i]._threads = set[i].d.number;
		}
		break;
	case STRESSD_PROP:
		for (i = 0; i < n_unit; i++) {
			set[i]._threads =  (set[i].d.proportion * 100) / (cfg->threads * 100);
			threads += set[i]._threads;
		}
		/* if it's not a multiple of 100, just round it */
		if (threads < cfg->threads)
			set[0]._threads += (cfg->threads - threads);
		break;
	}
	if (threads == 0)
		return EINVAL;

	/* allocating private structures */
	priv = calloc(sizeof(*priv), threads);
	if (priv == NULL)
		return ENOMEM;

	for (i = 0; i < threads; i++) {
		priv[i].fnpriv = fnpriv;
		priv[i].count = &count;

		/* prepare the conditional wait if needed */
		if (cfg->stress_type == STRESST_HORDE) {
			priv[i].cond = &cond;
			priv[i].mutex = &mutex;
			priv[i].wait = 1;
		}

		switch (cfg->distribution) {
		case STRESSD_RANDOM:
		/* if units have chance to happen */
			highest = -1;
			for (tries = 50; tries; tries--) {
				int r;
				r = rand();
				for (u = 0; (u < n_unit); u++) {
					if (!(r % (11 - set[u].d.chance))) {
						if (highest == -1)
							highest = u;
						else if (set[u].d.chance > set[highest].d.chance)
							highest = u;
					}
				}
				if (highest != -1)
					break;
			}
			if (!tries)
				highest = 0;
			priv[i].u = &set[highest];
			break;
		/* fixed number of unit runs. proportional has it
		 * pre-calculated */
		case STRESSD_FIXED:
		case STRESSD_PROP:
			for (u = 0; u < n_unit; u++)
				if (set[u]._threads)
					break;
			if (u == n_unit) {
				fprintf(stderr, "Internal error: %s:%i\n",
					__FILE__, __LINE__);
				exit(1);
			}
			priv[i].u = &set[u];
			set[u]._threads--;
			break;
		}
	}

	/* all initialized, start the threads */
	for (i = 0; i < threads; i++) {
		if (pthread_create(&priv[i].thread, NULL, _stresser_unit,
		    &priv[i])) {
			int e = errno;
			while (--i >= 0)
				pthread_cancel(priv[i].thread);
			return e;
		}
	}

	/* wait for all threads to start then release them all */
	if (cfg->stress_type == STRESST_HORDE) {
		while (1) {
			pthread_mutex_lock(&mutex);
			if (count == threads) {
				pthread_cond_broadcast(&cond);
				pthread_mutex_unlock(&mutex);
				break;
			}
			pthread_mutex_unlock(&mutex);
			pthread_yield();
		}
	}

	for (i = 0; i < threads; i++) {
		if (!pthread_join(priv[i].thread, NULL))
			if (priv[i].fnrc) {
				fprintf(stderr, "Thread %i returned error "
					"(%i)\n", i, priv[i].fnrc);
				rc = 1;
			}
	}
	return rc;
}
/* */

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

