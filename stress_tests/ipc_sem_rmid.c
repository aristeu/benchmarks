#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include "stresser.h"

static struct priv_info {
	int sem_id;
} priv_info;

static int sem_get_thread(void *priv)
{
	struct priv_info *info = priv;
	struct sembuf sem;

	while (1) {
		memset(&sem, 0, sizeof(sem));
		sem.sem_num = info->sem_id;
		sem.sem_op = -1;
		sem.sem_flg = SEM_UNDO;
		semop(info->sem_id, &sem, 1);
	}
	return 0;
}

static int rmid_thread(void *priv)
{
	struct priv_info *info = priv;
	unsigned short values[1] = {0};

	while (1) {
		sleep((random() % 5) + 1);
		semctl(info->sem_id, 0, IPC_RMID, NULL);
		info->sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT);
		semctl(info->sem_id, 0, SETALL, values);
	}
	return 0;
}

static int exit_thread(void *priv)
{
	sleep((random() % 10) + 1);
	exit(1);
}

static struct stresser_config config = {
	.threads = 100,
	.stress_type = STRESST_HORDE,
	.distribution = STRESSD_FIXED,
};

static struct stresser_unit units[] = {
	{ .d.number = 1, .fn = rmid_thread, },
	{ .d.number = 98, .fn = sem_get_thread, },
	{ .d.number = 1, .fn = exit_thread, },
};

static int launcher(struct stresser_config *config,
		    struct stresser_unit *units, int num)
{
	return stresser(config, units, num, &priv_info);
}

int main(int argc, char *argv[])
{
	pid_t pid;
	int status, cycles = -1;

	if (argc > 1)
		cycles = atoi(argv[1]);

	while (cycles--) {
		pid = fork();
		if (pid == -1) {
			perror("fork()");
			exit(1);
		}
		if (pid == 0)
			return launcher(&config, units, 3);
		wait(&status);
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	return 0;
}

