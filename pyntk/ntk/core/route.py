##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
#
# This source code is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This source code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# Please refer to the GNU Public License for more details.
#
# You should have received a copy of the GNU Public License along with
# this source code; if not, write to:
# Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

from ntk.core.map import Map
from ntk.lib.event import Event
from ntk.lib.rencode import serializable

class RemError(Exception):
    '''General Route Efficiency Measure Exception'''

class AvgError(RemError):
    '''Average Exception'''

class AvgSumError(AvgError):
    '''Average Sum Error'''

class RouteGwError(Exception):
    '''General RouteGw Error'''

class Rem(object):
    """Route Efficiency Measure.

    This is a base class for different metrics (rtt, bandwidth, ...)"""

    __slots__ = ['value', 'max_value', 'avgcoeff']

    def __init__(self, value=None, max_value=0, avgcoeff=1):
        self.value = value
        self.max_value = max_value        # Maximum value assumed by this REM
        self.avgcoeff = avgcoeff          # Coefficient used for the average

    def _pack(self):
        return (self.value, self.max_value, self.avgcoeff)

    def __cmp__(self, b):
        """Compares two REMs
        if remA > remB, then remA is better than remB

        self < b   -1    -->  The rem `self' is worse  than `b'
        self > b    1    -->  The rem `self' is better than `b'
        self == b   0    -->  They are the same

        NOTE: this means that if you have a list of rems and you
        want to sort it in decrescent order of efficiency, than you
        have to reverse sort it: list.sort(reverse=1)
        """
        return (self.value > b.value) - (self.value < b.value)

    def __add__(self, b):
        """It sums two REMs.

        The sum must be commutative, i.e. Rx+Ry=Ry+Rx"""
        raise NotImplementedError

    def __repr__(self):
        return '<%s: %s>' % (self.__class__.__name__, self.value)

class NullRem(Rem):
    """The equivalent of None for the REM"""

    def __add__(self, b):
            return b
    def __radd__(self, b):
            return b

serializable.register(NullRem)

class DeadRem(Rem):
    """A route with this rem is dead"""
    def __add__(self, b):
        return self

serializable.register(DeadRem)

class Rtt(Rem):
    """Round Trip Time"""

    def __init__(self, value, max_value=60*1000, avgcoeff=1): # 1 minute in ms
        Rem.__init__(self, value, max_value, avgcoeff)

    def __cmp__(self, b):
        """rtt comparison

        The comparison's semantic is reversed:
        if the first rtt is worse (bigger) than the second we will have:
        rem(rtt1) < rem(rtt2)

        self.value <  b.value   1   ->  The rtt `self' is better than `b' -> self > b
        self.value >  b.value  -1   ->  The rtt `self' is worse  than `b' -> self < b
        self.value == b.value   0   ->  They are the same -> self == b"""

        return (self.value < b.value) - (self.value > b.value)

    def __add__(self, b):
        if isinstance(b, DeadRem):
            return b + self
        elif isinstance(b, NullRem):
            return b + self
        elif isinstance(b, Rtt):
            return Rtt(self.value+b.value, self.max_value, self.avgcoeff)
        else:
            return NotImplemented

serializable.register(Rtt)

class Bw(Rem):
    """Bandwidth"""
    __slots__ = Rem.__slots__ + ['lb', 'nb']

    def __init__(self, value, lb, nb, max_value=0, avgcoeff=1):
        """Initialise the bandwidth Rem for a route.

        Let r be the route me->...->x, where x is a node. Let gw be the first
        hop after me in r, i.e r is me->gw->...->x. (note, gw is a neighbour
        of me).
        `value' is the total bw of the route r.
        `lb' is boolean: if 1, then the bandwidth of the link me->gw is the
        lowest in the route r.
        `nb' is the nearest bandwidth to bw(me->gw).
        See {-topodoc-} for more info.  """

        Rem.__init__(self, value, max_value, avgcoeff)

        self.lb = lb
        self.nb = nb

    def _pack(self):
        return (self.value, self.lb, self.nb, self.max_value, self.avgcoeff)

    def __add__(self, b):
        if isinstance(b, DeadRem):
            return b + self
        elif isinstance(b, NullRem):
            return b + self
        elif isinstance(b, Bw):
            return Bw(min(self.value, b.value), self.lb, self.nb,
                      self.max_value, self.avgcoeff)
        else:
            return NotImplemented

class Avg(Rem):
    """Average"""

    def __init__(self, rems):
        """Calculates the average of different REMs.

        `rems' is a list of type [R], where R is a Rem instance, f.e. Rtt.
        """

        length = sum = 0
        for r in rems:
            if not isinstance(r, Rem):
                raise RemError, "an element of `rems' is not a Rem instance"

            sum += abs(r.max_value - r.value*r.avgcoeff)
            length += 1

        Rem.__init__(self, sum/length)      # ???: value is always an integer?

    def __add__(self, b):
        raise AvgSumError('the Avg metric cannot be summed.'
                          ' It must be computed each time')

class RouteGw(object):
    """A route to a known destination.

    This class is intended for routes pointing to a same known destination d.
    The only variables here are `gw', the gateway of the route, and `rem',
    its Rem.

    If d = `gw', then `gw' is one of our internal neighbours, i.e. a neighbour
    belonging to our gnode of level 1.
    """
    __slots__ = ['gw', 'rem']

    def __init__(self, gw, rem):
        """New gw route"""
        self.gw = gw
        self.rem = rem

    def __cmp__(self, b):
        """The route self is better (greater) than b if its rem is better"""
        if isinstance(b, RouteGw):
            return self.rem.__cmp__(b.rem)
        else:
            raise RouteGwError, 'comparison with not RouteGw'

    def rem_modify(self, new_rem):
        """Sets self.rem=new_rem and returns the old rem"""
        if self.rem != new_rem:
            oldrem = self.rem
            self.rem = new_rem
            return oldrem
        return self.rem

class RouteNode(object):
    """List of routes to a known destination.

    This class is basically a list of RouteGw instances, where the
    destination node and its level are fixed and known.

    Note: for each gateway G there's only one route in self.routes, 
          which has the same gateway G
    """

    __slots__ = ['routes', 'routes_tobe_synced']

    def __init__(self, 
                 lvl=None, id=None  # these are mandatory for Map.__init__(),
                                    # but they aren't used
                ):
        self.routes = []
        self.routes_tobe_synced = 0     # number of routes to update in the kernel
        #TODO: keep the right track of `self.routes_tobe_synced'
        #      maybe it's better to use "self.routes_tobe_synced+-=1" before
        #      sending the ROUTE_NEW/ROUTE_DELETED/ROUTE_REM_CHGED events?

    def route_getby_gw(self, gw):
        """Returns the route having as gateway `gw'"""
        for r in self.routes:
            if r.gw == gw:
                return r
        return None

    def route_rem(self, gw, newrem):
        """Changes the rem of the route with gateway `gw'

        Returns (0, None) if the route doesn't exists, (1, oldrem) else."""

        r = self.route_getby_gw(gw)

        if r is None:
            return (0, None)

        oldrem = r.rem_modify(newrem)
        self.sort()
        self.routes_tobe_synced+=1 
        return (1, oldrem)

    def route_add(self, lvl, dst, gw, rem):
        """Add a route.

        It returns (0,None) if the route hasn't been added, and thus it isn't
        interesting, otherwise it returns (1,None) if it is a new route, 
        (2, oldrem) if it substituted an old route."""

        ret = 0
        val = None
        oldr = self.route_getby_gw(gw)

        if self.is_empty() or (oldr is None and rem > self.routes[-1].rem):
            # If there aren't routes, or if it is better than the worst
            # route, add it
            self.routes.append(RouteGw(gw, rem))
            ret = 1
        elif oldr is not None and rem > oldr.rem:
            # We already have a route with gateway `gw'. However, the new
            # route is better. Let's update the rem.
            oldrem = oldr.rem_modify(rem)
            val = oldrem
            ret = 2
        else:
            return (ret, val) # route not interesting

        self.routes_tobe_synced+=1

        self.sort()

        return (ret, val)         # good route

    def route_del(self, gw):
        """Delete a route.

        Returns 1 if the route has been deleted, otherwise 0"""

        r = self.route_getby_gw(gw)
        if r is not None:
            self.routes_tobe_synced+=1
            self.routes.remove(r)
            return 1
        return 0

    def route_reset(self):
        """Delete all the routes"""
        self.routes = []

    def sort(self):
        '''Order the routes

        Order the routes in decrescent order of efficiency, so that
        self.routes[0] is the best one
        '''
        self.routes.sort(reverse=1)

    def is_empty(self):
        return self.routes == []

    def is_free(self):
        '''Override the is_free() method of DataClass (see map.py)'''
        return self.is_empty()

    def nroutes(self):
        return len(self.routes)

    def nroutes_synced(self):
        # Note: it can be < 0
        return len(self.routes)-self.routes_tobe_synced

    def best_route(self):
        if self.is_empty():
            return None
        else:
            return self.routes[0]

def ftrue(*args):return True

class MapRoute(Map):
    """Map of routes, all of a same Rem type.

    MapRoute.node[lvl][id] is a RouteNode class, i.e. a list of routes
    having as destination the node (lvl, id)"""

    __slots__ = Map.__slots__ + ['remotable_funcs']

    def __init__(self, levels, gsize, me):

        Map.__init__(self, levels, gsize, RouteNode, me)

        self.events.add( [  'ROUTE_NEW',
                            'ROUTE_DELETED',
                            'ROUTE_REM_CHGED'   # the route's rem changed
                         ] )
        self.remotable_funcs = [self.free_nodes_nb]

    def route_add(self, lvl, dst, gw, rem, silent=0):
        ''' Add a new route
        '''
        n = self.node_get(lvl, dst)
        ret, val = n.route_add(lvl, dst, gw, rem)
        if not silent:
            if ret == 1:
                self.events.send('ROUTE_NEW', (lvl, dst, gw, rem))
                if n.nroutes() == 1:
                    # The node is new
                    self.node_add(lvl, dst)
            elif ret == 2:
                oldrem = val
                self.events.send('ROUTE_REM_CHGED', (lvl, dst, gw, rem, oldrem))
        return ret

    def route_del(self, lvl, dst, gw, silent=0):
        d = self.node_get(lvl, dst)
        d.route_del(gw)

        if not silent:
            self.events.send('ROUTE_DELETED', (lvl, dst, gw))

        if d.is_empty():
            # No more routes to reach the node (lvl, dst).
            # Consider it dead
            self.node_del(lvl, dst)

    def route_rem(self, lvl, dst, gw, newrem, silent=0):
        """Changes the rem of the route with gateway `gw'

        Returns 0 if the route doesn't exists, 1 else."""

        d = self.node_get(lvl, dst)
        ret, val = d.route_rem(gw, newrem)
        if ret:
            oldrem = val
            if not silent:
                self.events.send('ROUTE_REM_CHGED',
                                 (lvl, dst, gw, newrem, oldrem))
            return 1
        else:
            return 0

    def route_change(self, lvl, dst, gw, newrem):
        """A wrapper to route_add, route_del"""

        if isinstance(newrem, DeadRem):
            return self.route_del(lvl, dst, gw)
        else:
            return self.route_add(lvl, dst, gw, newrem)


## Neighbour stuff

    def routeneigh_del(self, neigh):
        """Delete from the MapRoute all the routes passing from the
           gateway `neigh.id' and delete the node `neigh' itself (if present)"""

        for lvl in xrange(self.levels):
            for dst in xrange(self.gsize):
                self.route_del(lvl, dst, neigh.id, silent=1)

    def routeneigh_add(self, neigh, silent=0):
        """Add a route to reach the neighbour `neigh'"""
        lvl, nid = self.routeneigh_get(neigh)
        return self.route_add(lvl, nid, neigh.id, neigh.rem, silent)

    def routeneigh_rem(self, neigh, silent=0):
        lvl, nid = self.routeneigh_get(neigh)
        return self.route_rem(lvl, nid, neigh.id, neigh.rem, silent)


    def routeneigh_get(self, neigh):
        """Converts a neighbour to a (g)node of the map"""
        neigh.nip = self.ip_to_nip(neigh.ip)   # TODO: refactoring Neigh
        lvl = self.nip_cmp(self.me, neigh.nip)
        return (lvl, neigh.nip[lvl])

    def bestroutes_get(self, f=ftrue):
        """Returns the list of all the best routes of the map.

           Let L be the returned list, then L[lvl] is the list of all the best
           routes of level lvl of the map. An element of this latter list is a 
           tuple (dst, gw, rem), where dst is the destination of the route, gw
           its gateway.

           If a function `f' has been specified, then each element L[lvl][i]
           in L is such that f(L[lvl][i])==True
           """
        return [
                [ (dst, br.gw, br.rem)
                        for dst in xrange(self.gsize)
                            for br in [self.node_get(lvl, dst).best_route()]
                                if br is not None and f((dst, br.gw, br.rem))
                ] for lvl in xrange(self.levels)
               ]
