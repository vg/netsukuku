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
 */

#ifndef EVENT_H
#define EVENT_H

/*\
 * 
 * 			Event API
 * 		      =============
 * Event flow
 * ----------	
 *
 * Suppose we are dealing with the NEW_FOO event. There might be different
 * modules interested in this event. Each of them will register itself as a 
 * listener.
 * A module, to become a listener, associates its listening function to the
 * event NEW_FOO. Suppose that the modules mod1, mod2, mod3 registers
 * respectively the following functions:
 *
 * 	mod1_exec_func()	mod2_exec_func() 	mod3_exec_func()
 *
 * At some point a module triggers the NEW_FOO event. The event is
 * pushed in the event queue. The event is then fetched from the queue and 
 * dispatched to the listening functions. In this example, the listening
 * functions will be called in the following order:
 *
 * 	mod1_exec_func(NEW_FOO, event_data);
 * 	mod2_exec_func(NEW_FOO, event_data);
 * 	mod3_exec_func(NEW_FOO, event_data);
 * 
 * If a listening function returns EV_DROP, then the other listners won't
 * receive the event, which will be silently droppped. For this reason, the
 * calling order of the listening function is relevant. 
 * Their order of execution can be changed by assigning different priority
 * values to each listening function. For example, with the priorities 
 * 4, 5, 3 assigned respectively to mod1, mod2, mod3, the calling order would
 * be:
 *
 * 	mod2_exec_func(NEW_FOO, event_data);
 * 	mod1_exec_func(NEW_FOO, event_data);
 * 	mod3_exec_func(NEW_FOO, event_data);
 * 	
 * It's impossible to predict the order of execution of two or more functions
 * with the same priority value.
 *
 *
 * Registering a new event
 * -----------------------
 *
 *  ....
 *
 *  The listener should always free `data'!!
\*/

#include "llist.c"

typedef int32_t ev_t;


/*
 * Return values of the listener() function
 */
#define EV_PASS		 0		/* Pass the event to the successive
					   listeners */
#define EV_DROP		-2		/* Drop the event. Don't pass it to
					   successive listeners */


/*
 * ev_listenerfuncs.flags
 */
#define EV_LISTENER_BlOCK	1	/* Before calling the next listener in
					   turn, wait that the return of the 
					   called function of current one. 
					   This is the default. */
#define EV_LISTENER_NONBlOCK	(1<<1)


struct ev_listener
{
	LLIST_HDR	(struct ev_listener);

	u_char		priority;	/* The priority assigned to this
					   event listener */
	u_char		flags;

	/* 
	 * The function of the listener, which is executed when the
	 * registered event is triggered and when the turn of this listener
	 * comes.
	 */
	int		(*listener)(ev_t event, void *data, size_t data_sz);
};
typedef struct ev_listener ev_listener;

/*
 * event_tbl
 * ---------
 *
 * The event_tbl keeps all the registered events.
 * Each event has its associated listeners.
 */
typedef struct
{
	ev_t		hash;		/* Hash of the event name */

	const char	*name;		/* Name of the event */

	ev_listener	*listener;	/* The listeners of this event */

	u_char		flags;

} ev_tbl;


struct ev_queue_entry
{
	LLIST_HDR	(struct ev_queue_entry);

	ev_t		hash;		/* Event name hash */
	void		*data;		/* Data associated to this event.
				           This must be a pointer to a 
					   mallocated buffer, since it will be 
					   freed upon the event destruction */
	size_t		data_sz;
};
typedef struct ev_queue_entry ev_queue_entry;

/*
 * ev_queue
 * --------
 *
 * The event queue. Basically a FIFO.
 * Each new event is pushed into an event queue. Then a dispatcher takes care
 * of delivering the events stored in the queue.
 */
typedef struct
{
	u_short		max_events;	/* Maximum number of events allowed in 
					   this queue. Past this point all the
					   new events will be dropped */
	
	ev_queue_entry	*qhead;		/* The queue's head */
	ev_queue_entry  *qtail;		/* its tail */
	int		qcounter;	/* number of elements in the queue */

	u_char		dispatching;	/* If non-zero, there's some dispatcher 
					   thread active currently handling 
					   this queue */
	pthread_t	dispatcher_t;
	pthread_mutex_t mutex_t;
	pthread_mutex_t tail_mutex;	/* Write mutex for `qtail' */
} ev_queue;

/*\
 *
 * Functions declaration
 *
\*/

ev_t ev_hash_name(const char *ev_name);
void ev_sort_events(void);

#define EV_REG_EVENT(_ev, _flags)	_ev = ev_register_event(#_ev , (_flags))
int 	ev_register_event(const char *ev_name, u_char flags);
void 	ev_del_event(int ev_hash);

ev_tbl *ev_get_evstruct(int ev_hash);


int ev_listen_event(ev_t event,
                    int (*listener)(ev_t, void *, size_t), u_char priority,
                    u_char flags);
int ev_ignore_event(ev_t event, int (*listener)(ev_t, void *, size_t));

void ev_init_event_queue(ev_queue *q, int max_events);
void ev_free_event_queue(ev_queue *q);

int ev_trigger_event(ev_queue *q, ev_t event, void *data, 
		     size_t data_sz, u_char detach);

#endif /*EVENT_H*/
