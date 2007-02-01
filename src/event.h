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
 * 							|{event_api}|
 * 			Event API
 * 		      =============
 *
 * 1. Event flow		  {-event-flow-}
 * 2. Registering a new event	  {-event-register-}
 * 3. Listening to an event	  {-event-listen-}
 * 4. Triggering an event	  {-event-trigger-}
 *
 *
 * Event flow						|{event-flow}|
 * ----------	
 *
 * Suppose we are dealing with the NEW_FOO event, which has been already
 * registered as a valid event.
 * There might be different modules interested in this event. Each of
 * them will register itself as a listener.
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
 * Registering a new event 				|{event-register}|
 * -----------------------
 *
 * The name of the event has to be in this format:
 *
 * 		ID_EVENTNAME
 *
 * where ID is the short name of your module (in upper case), and
 * EVENTNAME is the name of the event (always in upper case). 
 * F.e:
 * 		FOO_MAP_UPDATED
 *
 * The ev_register_event() function is used to register a new event.
 * Its returned value is the numeric id assigned to the event.
 * It must be saved in a global variable (non static), in this way, even other
 * modules we'll be able to use it. Use the EV_REG_EVENT() macro
 * to facilitate this assignment.
 * The global variable name must the same of the event name (always in upper
 * case).
 * All the other functions of the NTK code which deals with events, will
 * take as arguments this numeric id.
 * See event.c for the complete description ev_register_event().
 *
 * An event must be registered only once, therefore the best strategy is
 * to call ev_register_event() inside an initialisation function.
 * 
 * If you are implementing a module, you should use the ev_del_event(),
 * otherwise if the module will be closed and then loaded a second
 * time, fatal() will be called, because event.c would think that a
 * name collision happened.
 *
 * When registering a new event, you must be sure that it can be sent to an
 * event queue. If there are no event queues suitable for the new event, you
 * must create one with ev_init_event_queue(). Remember to keep the event
 * queue global and to call ev_free_event_queue() once it is of no more use.
 *
 * Example:
 * 	
 * 	struct ev_queue my_event_queue;
 *
 * 	EV_REG_EVENT(FOO_MAP_UPDATED, 0);
 * 	ev_init_event_queue(&my_event_queue);
 *
 *
 * Listening to an event				|{event-listen}|
 * ---------------------
 * 
 * A module can listen to an event by calling the ev_listen_event() function.
 * Its prototype is:
 *
 *	int ev_listen_event(ev_t event, 
 *		    	    int (*listener)(ev_t, void *, size_t), 
 *		    	    u_char priority,
 *		            u_char flags)
 *
 * The second argument passed to this function must be a pointer to the
 * listening function. The listening function is called when the event is
 * triggered. The prototype of the listening function is:
 *
 * 	int *listener(ev_t event, void *data, size_t data_sz)
 *
 * `data' is pointer to a mallocated buffer which contains `data_sz' bytes of
 * data associated to the event. If data_sz > 0, then the listening function
 * must always free `data'!
 * The listening function shall return EV_PASS or EV_DROP. EV_DROP will make
 * the event immediately drop, without forwading it to the other listening
 * functions in queue.
 *
 * The `priority' number, as explained earlier, affects the order of execution
 * of the listening functions associated to the event `event'. 255 is the
 * highest priority, while 0 the lowest.
 *
 * The `flags' specifies how the listening function is executed. If it is 0
 * the listening function will be called directly inside the
 * dispatcher thread, thus the dispatcher, before calling the next listener in
 * turn, waits the return of the called function.
 * If, instead, the EV_LISTENER_NONBlOCK flag is set, the dispatcher will
 * create a new thread and inside it the listening function will be executed.
 *
 *
 * The opposite function of ev_listen_event() is ev_ignore_event() (see event.c).
 *
 * Example:
 *
 * 	int foo_listener(ev_t event, void *data, size_t data_sz)
 *	{
 *		loginfo("Event %s received, with a paylod of %d bytes",
 *				ev_to_str(event), data_sz);
 *		if(data)
 *			xfree(data);
 *
 *		return EV_PASS;
 *	}
 *	
 *	init_my_mod() {
 *		...
 *
 *		ev_listen_event(FOO_MAP_UPDATED,
 *				foo_listener, 50,
 *				EV_LISTENER_NONBlOCK);
 *
 *	}
 *
 * Triggering an event					|{event-trigger}|
 * -------------------
 *
 * The function ev_trigger_event() is used to trigger events:
 *
 *	int ev_trigger_event(ev_queue *q, ev_t event, 
 *			     void *data, size_t data_sz, 
 *			     u_char detach);
 *
 * `q' is a pointer to an event_queue initialized with ev_init_event_queue().
 * `data' can be NULL or a pointer to a buffer which holds `data_sz' bytes.
 * The buffer will be copied into a new mallocated array, thus it is safe to
 * modify and free `data' after the call.
 *
 * If `detach' is not zero, the dispatcher of the event queue is executed on a
 * new thread, otherwise the dispatcher is called from this same function and
 * as a consequence, this function will block until the entire event queue
 * is processed.
 * 
\*/

#include "llist.c"

typedef int32_t ev_t;


/*
 * Return values of the listener() function
 */
#define EV_PASS		 0		/* Pass the event to the successive
					   listeners */
#define EV_DROP		-1		/* Drop the event. Don't pass it to
					   successive listeners */


/*
 * ev_listenerfuncs.flags
 */
#define EV_LISTENER_NONBlOCK	1	/* A new thread is created to execute 
					   the listening function */

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
const u_char *ev_to_str(ev_t ev_hash);

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
