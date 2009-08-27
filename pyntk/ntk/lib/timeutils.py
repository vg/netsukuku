import ntk.wrap.xtime as xtime
from datetime import date, timedelta

def now():
    return xtime.time()

def timestamp_to_data(timestamp):
    return date.fromtimestamp(timestamp)
    
def today():
    #note: we can't use date.today() because we need to wrap 
    #      the `time' module with `xtime'.
    return timestamp_to_data(now())

def days(number=3):
    return timedelta(days=number)
