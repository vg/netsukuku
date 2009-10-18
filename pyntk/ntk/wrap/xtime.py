import time as T
import ntk.lib.micro as micro

def swait(t):
    """Waits `t' ms"""

    final_time = T.time()+t/1000.
    while final_time >= T.time():
        T.sleep(0.001)
        micro.micro_block()

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

    if not repetitions:
        while True:
            if func():
                    return True
    i=0
    while i < repetitions:
            if func():
                    return True
            swait(wait_millisec)
            i+=1
    return False

def time():
    return int(T.time()*1000)
