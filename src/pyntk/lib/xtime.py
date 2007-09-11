import time

def swait(self, t):
    """Waits `t' ms"""

    time.sleep(t/1000.)

def time(self):
    return int(time.time()*1000)
