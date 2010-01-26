import time as T
import ntk.lib.micro as micro

from datetime import date, timedelta

def swait(t):
    """Waits `t' ms"""

    micro.time_swait(t)

def sleep_during_hard_work(t=0):
    """This is just a wrapper to `swait'. It is meant to be used during time
       consuming operations, in order to give the possibilities to other
       microthreads to run (for example, microthreads involving sockets)

       sleep_during_hard_work(0) is equivalent to calling micro.micro_block()
    """
    if not t:
        micro.micro_block()
    else:
        swait(t)

def while_condition(func, wait_millisec=10, repetitions=0):
    """If repetitions=0, it enters in an infinite loop checking each time
       if func()==True, in which case it returns True.
    If repetitions > 0, then it iterates the loop for N times, where
    N=repetitions. If func() failed to be True in each iteration, False is
    returned instead.

    :param wait_millisec: number of milliseconds to wait at the end of
                          each iteration.
    """

    micro.time_while_condition(func, wait_millisec, repetitions)

def time():
    return int(T.time()*1000)

def now():
    return T.time()

def timestamp_to_data(timestamp):
    return date.fromtimestamp(timestamp)
    
def today():
    return date.today()

def days(number=3):
    """ Take the number of days and return an object that 
    can be used for comparison with data objects """
    return timedelta(days=number)
