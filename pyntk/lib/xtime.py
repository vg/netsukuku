import time as T
import micro

def swait(t):
    """Waits `t' ms"""

    final_time = T.time()+t/1000.
    while final_time >= T.time():
	    T.sleep(0.001)
	    micro.micro_block()

def time():
    return int(T.time()*1000)
