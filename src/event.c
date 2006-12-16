/* This file is part of Netsukuku
 * (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * --
 * event.c
 *
 * General library used to register, trigger and receive events.
 * One or more functions of different modules are attached to a particular
 * event. When that event occurs, they will be called in order. For example,
 * when a packet is received, the event EV_RECVD_PACKET is triggered, and the
 * function exec_pkt() is be called.
 *
 * For more information see event.h
 */

#include "includes.h"

#include "hash.h"
#include "event.h"
#include "xmalloc.h"
#include "log.h"

/*
 * The global event table
 */
ev_tbl *ntk_event;
int ntk_ev_counter=0;
u_char ntk_event_sorted=0;

/*
 * ev_hash_name
 *
 * It return the 32bit hash of `ev_name'.
 * The returned hash is always != 0.
 */
ev_t ev_hash_name(const char *ev_name)
{
	ev_t hash;

	hash=(ev_t)fnv_32_buf((u_char *)ev_name, 
			       strlen(ev_name),
			       FNV1_32_INIT);

	return !hash ? hash+1 : hash;
}

int ev_hash_cmp(const void *a, const void *b)
{
	ev_tbl *ai=(ev_tbl *)a, *bi=(ev_tbl *)b;

	return (ai->hash > bi->hash) - (ai->hash < bi->hash);
}

/*
 * ev_lsearch_hash
 *
 * Returns the index number of the struct. contained in the `ntk_event'
 * array, which has the same `ev_hash'.
 *
 * If nothing was found, -1 is returned.
 */
int ev_lsearch_hash(const ev_t ev_hash)
{
	int i;

	for(i=0; i<ntk_ev_counter; i++)
		if(ntk_event[i].hash == ev_hash)
			return i;

	return -1;
}

/*
 * ev_bsearch_hash
 *
 * Performs a bsearch(3) on the `ntk_event' array searching for a structure
 * which has the same hash of `ev_hash'.
 *
 * If the struct is found its index number is returned,
 * otherwise -1 is returned.
 */
int ev_bsearch_hash(const ev_t ev_hash)
{
	ev_tbl *ev;
	ev_tbl ev_tmp={.hash = ev_hash};

	if(!ntk_event_sorted)
		ev_sort_requests();

	if((ev=bsearch(&ev_tmp, ntk_event, ntk_ev_counter, 
			sizeof(ev_tbl), ev_hash_cmp)))
		return ((char *)ev-(char *)ntk_event)/sizeof(ev_tbl);

	return -1;
}

/*
 * ev_sort_requests
 *
 * Sorts the `ntk_event' array using the 32bit hashes.
 *
 * Once the array has been sorted, it is possible to use the
 * ev_bsearch_hash() function, which is far more efficient than
 * ev_lsearch_hash().
 *
 * This function should be called after all the events have been added to
 * the array with ev_register_event().
 */
void ev_sort_requests(void)
{
	qsort(ntk_event, ntk_ev_counter,
			sizeof(ev_tbl), ev_hash_cmp);
	ntk_event_sorted=1;
}

/*
 * ev_register_event
 * -----------------
 *
 * Registers the event named `ev_name'. The name should be all in upper case.
 *
 * The hash of `ev_name' is returned. It is a 32bit integer of type `ev_t'.
 *
 * The hash must be saved for future use, because all the functions, which
 * deals with events, will need it as argument.
 * The best way is to save it in a global variable, with the name all in
 * upper case, which has been declared in the header file (.h). In this way,
 * even the other source codes will be able to use it.
 * To avoid conflicts, the name of the variable and of the event must be
 * prefixed with a unique identifier.
 * It's adviced to use the name of the relative source code.
 *
 * If an error occurred, fatal() is called, because this is a function which
 * must never fail.
 *
 *  :Warning: 
 * 	 
 * 	 * Use ev_del_event() when the event won't be used anymore
 *
 *  :Warning:
 *
 */
int ev_register_event(const char *ev_name, u_char flags)
{
	ev_t hash;

	hash=ev_hash_name(ev_name);

	if(ev_lsearch_hash(hash) != -1)
		fatal(ERROR_MSG 
		      "The \"%s\" event has been already added or its hash "
		      "it's collinding with another event. "
		      "In the former case, avoid to register this event,"
		      "in the latter, change the name of the event",
		      ERROR_POS, ev_name);

	ntk_event=xrealloc(ntk_event, (ntk_ev_counter+1)*sizeof(ev_tbl));

	ntk_event[ntk_ev_counter].hash=hash;
	ntk_event[ntk_ev_counter].name=ev_name;
	ntk_event[ntk_ev_counter].flags=flags;
	
	ntk_ev_counter++;
	ntk_event_sorted=0;

	return hash;
}

/*
 * ev_del_event
 *
 * Removes the `ev_hash' event from the ntk_event array.
 */
void ev_del_event(ev_t ev_hash)
{
	int idx;

	if((idx=ev_bsearch_hash(ev_hash)) < 0)
		return;

	if(idx < ntk_ev_counter-1)
		/* Shifts all the succesive elements of `idx', in this way,
		 * the order of the array isn't changed */
		memmove(&ntk_event[idx], 
			  &ntk_event[idx+1],
				sizeof(ev_tbl)*(ntk_ev_counter-idx-1));
	ntk_ev_counter--;
	ntk_event=xrealloc(ntk_event, ntk_ev_counter*sizeof(ev_tbl));
}

/*
 * ev_get_evstruct
 *
 * Returns the pointer to the event structure which contains `ev_hash'.
 * If no structure is found, 0 is returned.
 */
ev_tbl *ev_get_evstruct(ev_t ev_hash)
{
	int i=ev_bsearch_hash(ev_hash);
	return i < 0 ? 0 : &ntk_event[i];
}

/*
 * ev_listener_cmp
 *
 * Used to sort the ev_listener llist by priority
 */
int ev_listener_cmp(const void *a, const void *b)
{
	ev_listener *la=*(ev_listener **)a;
	ev_listener *lb=*(ev_listener **)b;

	return (la->priority > lb->priority) - (la->priority < lb->priority);
}

/*
 * ev_listen_event
 * ---------------
 *
 * Associate the listening function `listener' with the specified `priority'
 * to `event', in this way, when `event' will be triggered, `listener' will be
 * called.
 *
 * If `event' hasn't been registered in the event table, -1 is returned,
 * otherwise 0 is the returned value.
 */
int ev_listen_event(ev_t event, 
		    int (*listener)(ev_t, void *), u_char priority,
		    u_char flags)
{
	ev_tbl *evt;

	if(!(evt=ev_get_evstruct(event)))
		return -1;

	ev_listener *evl = xzalloc(sizeof(ev_listener));
   	evl.listener=listener;
	evl.priority=priority;
	evl.flags=flags;

	/* Add the new listener and sort the llist by priority */
	evt->listener=list_add(evt->listener, evl);
	evt->listener=qsort(evt->listener, 0, ev_listener_cmp);

	return hash;
}

/*
 * ev_ignore_event
 * ---------------
 *
 * De-associate the listening function `listener' from `event'.
 *
 * If `event' hasn't been registered in the event table or if the `listener'
 * function has never been associated to `event', -1 is returned,
 * otherwise 0 is the returned value.
 */
int ev_ignore_event(ev_t event, int (*listener)(ev_t, void *))
{
	ev_tbl *evt;
	ev_listener *el, *next;

	if(!(evt=ev_get_evstruct(event)))
		return -1;

	el=evt->listener;
	list_safe_for(el, next) {
		if(el->listener == listener) {
			evt->listener=list_del(evt->listener, el);
			return 0;
		}
	}

	return -1;
}

/*
 * ev_init_event_queue
 *
 * Initializes the event queue `q', setting its capacity of maximum
 * number of events to `max_events'.
 */
void ev_init_event_queue(ev_queue *q, int max_events)
{
	q->max_events=max_events;
	q->qhead=q->qtail=clist_init(&q->queue_counter);
}

/*
 * ev_trigger_event
 *
 * Triggers the new event `event' and pushes it, along with its `data', in
 * the event queue `q'.
 * If `detach' is not zero, the dispatcher of the event queue is executed on a
 * new thread, otherwise the dispatcher is called from this same function and
 * as a consequence, this function will block until the entire event queue
 * is processed.
 *
 * If none listen to this event, no action is perfomed.
 *
 * If the queue is full -1 is returned.
 * On success 0 is returned.
 */
int ev_trigger_event(ev_queue *q, ev_t event, void *data, u_char detach)
{
	ev_queue_entry *qe;
	int i;

	if(q->queue_counter >= q->max_events)
		return -1;

	qe=xmalloc(sizeof(ev_queue_entry));
	qe->hash=event;
	qe->data=data;
	clist_append(&q->qhead, &q->qtail, &q->qcounter, qe);

	ev_event_dispatcher(q, detach);

	return 0;
}
