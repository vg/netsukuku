import sys
sys.path.append('..')
from ntk.lib.event import Event
from functools import partial


T=[]
events = Event(['A', 'B'])
events.add(['C'])

def ev_listener(ev='', *msg):
        print ev, msg
        T.append(ev)

events.listen('A', partial(ev_listener, 'A'))
events.listen('B', partial(ev_listener, 'B'))
events.listen('C', partial(ev_listener, 'C'))

events.send('A', ())
events.send('B', ('test', 'test'))
events.send('C', (range(10)))

assert T == ['A', 'B', 'C']
