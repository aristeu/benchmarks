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

#ifndef STRESSER_H
#define STRESSER_H

#include <pthread.h>

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
		unsigned char chance;		/* unit chance 1-10 in 10 of running */
		unsigned char proportion;	/* 1-100 proportion this unit will run */
	} d;					/* which value is used depends upon 'distribution' in config */
	int (*fn)(void *priv);			/* function to be executed */
	void *fnpriv;				/* override the global private info for this unit */
	unsigned int _reserved;			/* do not use */
};

int stresser(struct stresser_config *cfg, struct stresser_unit *set,
	     int n_unit, void *fnpriv);

#endif	/* STRESSER_H */
