#include <stdio.h>
#include <fcntl.h>
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>

#define ARRAY_SIZE 500
#define DEFAULT_THREADS 50
#define MAX_TRIES 1

struct single_pty {
	int master;
	int slave;
	pthread_mutex_t lock;
} array[ARRAY_SIZE];

struct array_info {
	struct single_pty *array;
	int size;
};

static void *ptmx_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i, tries;

	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->master == -1) {
				for (tries = 0; tries < MAX_TRIES; tries++) {
					cur->master = open("/dev/ptmx", O_RDWR);
					if (cur->master >= 0)
						break;
				}
				if (tries == MAX_TRIES) {
					printf("*** cannot open /dev/ptmx: "
						"%s\n", strerror(errno));
					fflush(stdout);
					cur->master = -1;
				}
			}
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void *slave_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i, tries;
	char *ptsn;

	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->master != -1 && cur->slave == -1) {
				ptsn = ptsname(cur->master);
				if (ptsn == NULL)
					goto out;
				for (tries = 0; tries < MAX_TRIES; tries++) {
					cur->slave = open(ptsn, O_RDWR |
								   O_NONBLOCK);
					if (cur->slave >= 0)
						break;
				}
				if (tries == MAX_TRIES)
					;
			}
out:
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void *reader_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i; 
	char buff[200];

	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->master != -1 && cur->slave != -1)
				read(cur->slave, buff, sizeof(buff));
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void *writer_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i; 
	char buff[200];

	memset(buff, 'a', sizeof(buff));

	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->master != -1)
				write(cur->master, buff, sizeof(buff));
			if (cur->slave != -1)
				write(cur->slave, buff, sizeof(buff));
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void *ptmx_closer_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i; 
	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->master != -1) {
				close(cur->master);
				cur->master = -1;
			}
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void *slave_closer_thread(void *arg)
{
	struct array_info *info = arg;
	struct single_pty *array = info->array, *cur;
	int size = info->size, i; 
	while (1) {
		for (i = 0; i < ARRAY_SIZE; ) {
			cur = &array[i];
			pthread_mutex_lock(&cur->lock);
			if (cur->slave != -1) {
				close(cur->slave);
				cur->slave = -1;
			}
			pthread_mutex_unlock(&cur->lock);
			i += ((rand() % 10) + 1);
		}
	}
	return NULL;
}

static void sodomize_pty(struct single_pty *array, int size)
{
	int i;
	struct array_info info = { .array = array, .size = size };
	pthread_t last;

	printf("Creating threads...\n");
	fflush(stdout);
	for (i = 0; i < DEFAULT_THREADS; i++) {
		switch(rand() % 6) {
			case 0:
				if (pthread_create(&last, NULL,
						   ptmx_thread, &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
			case 1:
				if (pthread_create(&last, NULL,
						   slave_thread, &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
			case 2:
				if (pthread_create(&last, NULL,
						   reader_thread, &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
			case 3:
				if (pthread_create(&last, NULL,
						   writer_thread, &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
			case 4:
				if (pthread_create(&last, NULL,
						   ptmx_closer_thread,
						   &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
			case 5:
				if (pthread_create(&last, NULL,
						   slave_closer_thread,
						   &info)) {
					perror("Error creating thread: ");
					return;
				}
				break;
		}
	}
	pthread_join(last, NULL);
}

int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < ARRAY_SIZE; i++)
		pthread_mutex_init(&array[i].lock, NULL);
	sodomize_pty(array, ARRAY_SIZE);
}

