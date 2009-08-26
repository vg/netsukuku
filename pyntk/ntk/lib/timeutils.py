import time as T
from datetime import date, timedelta

def now():
    return T.time()

def timestamp_to_data(timestamp):
    return date.fromtimestamp(timestamp)
    
def today():
    return date.today()

def days(number=3):
    return timedelta(days=number)
