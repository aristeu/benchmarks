/*
 * stresser library - create many threads to stress a particular feature
 * Copyright (C) 2013  Aristeu Rozanski <aris@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include "stresser.h"

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

int stresser(struct stresser_config *cfg, struct stresser_unit *set,
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
			set[i]._reserved = set[i].d.number;
		}
		break;
	case STRESSD_PROP:
		for (i = 0; i < n_unit; i++) {
			set[i]._reserved =  (set[i].d.proportion * 100) / (cfg->threads * 100);
			threads += set[i]._reserved;
		}
		/* if it's not a multiple of 100, just round it */
		if (threads < cfg->threads)
			set[0]._reserved += (cfg->threads - threads);
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
				if (set[u]._reserved)
					break;
			if (u == n_unit) {
				fprintf(stderr, "Internal error: %s:%i\n",
					__FILE__, __LINE__);
				exit(1);
			}
			priv[i].u = &set[u];
			set[u]._reserved--;
			break;
		}
		if (priv[i].u->fnpriv)
			priv[i].fnpriv = priv[i].u->fnpriv;
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
