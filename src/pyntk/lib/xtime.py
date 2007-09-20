import time as t

def swait(t):
    """Waits `t' ms"""

    time.sleep(t/1000.)

def time():
    return int(t.time()*1000)
