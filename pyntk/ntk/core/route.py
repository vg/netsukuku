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

import ntk.lib.rpc as rpc

from ntk.core.map import Map
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.log import get_stackframes
from ntk.lib.rencode import serializable

class RemError(Exception):
    '''General Route Efficiency Measure Exception'''

class AvgError(RemError):
    '''Average Exception'''

class AvgSumError(AvgError):
    '''Average Sum Error'''

class RouteError(Exception):
    '''General Route Error'''

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

        self.value <  b.value   1   ->  The rtt `self' is better than `b'
        self.value >  b.value  -1   ->  The rtt `self' is worse  than `b'
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
                raise RemError("an element of `rems' is not a Rem instance")

            sum += abs(r.max_value - r.value * r.avgcoeff)
            length += 1

        Rem.__init__(self, sum/length) # ???: value is always an integer?

    def __add__(self, b):
        raise AvgSumError('the Avg metric cannot be summed.'
                          ' It must be computed each time')

class Route(object):
    """A path to a known destination.

    An instance of this class represents a path to a known 
    destination d.
    The instance members are:
    `gw': the gateway of the route. Our next hop. It is an instance of the
          class Neigh in module radar.py
    `rem_at_gw': a REM (Route Efficience Measure) of the path from the 
          gateway to the destination d. It is an instance of the class 
          Rem or a derivative.
    `hops': a sequence of pairs (lvl, id) of the hops represented by this
            path from the gateway gw to the destination d.
            gw and d are not included.
    """
    __slots__ = ['gw', 'rem_at_gw', 'hops', 'rem']

    def __init__(self, gw, rem_at_gw, hops):
        """New gw route"""
        self.gw = gw
        self.rem_at_gw = rem_at_gw
        self.hops = hops

    def update_gw(self, gw):
        """Update Neigh data of my gateway"""
        self.gw = gw

    def _get_rem(self):
        return self.rem_at_gw + self.gw.rem

    rem = property(_get_rem)

    def __cmp__(self, b):
        """The route self is better (greater) than b if its rem is better"""
        if isinstance(b, Route):
            return self.rem.__cmp__(b.rem)
        else:
            raise RouteError('comparison with not Route')

    def __eq__(self, b):
        '''The route self is equal to route b'''
        if isinstance(b, Route):
            # It is used to test between 2 routes in the same RouteNode.
            #  (e.g. to remove a route from the sequence of routes)
            # In such a comparison the only member we can check is the 
            # gateway.
            return self.gw.id == b.gw.id
        else:
            return False

    def rem_modify(self, newrem_at_gw, new_hops):
        """Sets new rem_at_gw and hops and returns the old rem_at_gw"""
        oldrem_at_gw = self.rem_at_gw
        self.rem_at_gw = newrem_at_gw
        self.hops = new_hops
        return oldrem_at_gw

    def __repr__(self):
        return '<Route: gw(%s), rem(%s), #hops(%s)>' % (self.gw.id, self.rem,
                                                        len(self.hops))

class RouteNode(object):
    """List of paths to a known destination.

    This class is basically a list of Route instances, where the
    destination node and its level are fixed and known.

    Note: for each gateway G there's only one route in self.routes,
          which has the same gateway G
    """

    __slots__ = ['routes', 'lvl', 'id', 'its_me']

    def __init__(self, lvl, id, its_me=False):
        self.lvl = lvl
        self.id = id
        self.its_me = its_me
        self.routes = []

    def route_getby_gw(self, gw_id):
        """Returns the route having as gateway `gw_id'"""
        for r in self.routes:
            if r.gw.id == gw_id:
                return r
        return None

    def route_update_gw(self, gw):
        """Updates the Neigh data of the route with gateway `gw'"""

        r = self.route_getby_gw(gw.id)

        if r is None:
            return 0

        r.update_gw(gw)
        self.sort()
        return 1

    def route_rem(self, gw_id, newrem_at_gw, new_hops):
        """Changes the rem_at_gw and hops of the route with gateway `gw_id'

        Returns (0, None) if the route doesn't exists, 
        (1, oldrem_at_gw) else."""

        r = self.route_getby_gw(gw_id)

        if r is None:
            return (0, None)

        oldrem_at_gw = r.rem_modify(newrem_at_gw, new_hops)
        self.sort()
        return (1, oldrem_at_gw)

    def route_add(self, lvl, dst, gw, rem_at_gw, hops):
        """Add a route.

        It returns (0,None) if the route hasn't been added, and thus it isn't
        interesting, otherwise it returns (1,None) if it is a new route,
        (2, oldrem_at_gw) if it substituted an old route."""

        ret = 0
        val = None
        oldr = self.route_getby_gw(gw.id)

        if oldr is None:
            # If there isn't a route through this gateway, add it
            self.routes.append(Route(gw, rem_at_gw, hops))
            ret = 1
            self.sort()
        elif rem_at_gw > oldr.rem_at_gw: 
            # At the moment this does not make sense alot.
            # We call route_add only to add a route through a gw that was not
            # there.
            logging.warning('RouteNode: route_add called, '
                            'but it was existing.')
            logging.warning(get_stackframes(back=1))
            # Anyway, this is not a changed rem, but a different route through
            # the same gateway. So we update iff the rem is better.
            oldrem_at_gw = oldr.rem_modify(rem_at_gw, hops)
            val = oldrem_at_gw
            ret = 2
            self.sort()

        return (ret, val)


    def route_del(self, gw_id):
        """Delete a route.

        Returns 1 if the route has been deleted, otherwise 0"""

        r = self.route_getby_gw(gw_id)
        if r is not None:
            self.routes.remove(r)
            return 1
        return 0

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
        if self.its_me: return False
        return self.is_empty()

    def nroutes(self):
        return len(self.routes)

    def best_route(self, exclude_gw_ids=[]):
        for r in self.routes:
            if r.gw.id not in exclude_gw_ids: return r
        return None

    def __repr__(self):
        return '<RouteNode: %s>' % self.routes

def ftrue(*args):
    return True

class MapRoute(Map):
    """Map of routes, all of a same Rem type.

    MapRoute.node[lvl][id] is a RouteNode class, i.e. a list of routes
    having as destination the node (lvl, id)"""

    __slots__ = Map.__slots__ + ['radar', 'remotable_funcs']

    def __init__(self, levels, gsize, me):

        self.radar = None
        Map.__init__(self, levels, gsize, RouteNode, me)

        self.events.add( [  'ROUTE_NEW',
                            'ROUTE_DELETED',
                            'ROUTE_REM_CHGED'   # the route's rem changed
                         ] )
        self.remotable_funcs = [self.free_nodes_nb, self.free_nodes_nb_udp]

    def set_radar(self, radar):
        self.radar = radar

    def call_free_nodes_nb_udp(self, neigh, lvl):
        """Use BcastClient to call highest_free_nodes"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'maproute.free_nodes_nb_udp', 
                            (lvl, ))

    def free_nodes_nb_udp(self, _rpc_caller, caller_id, callee_nip, 
                          callee_netid, lvl):
        """Returns the result of free_nodes_nb to remote caller.
           caller_id is the random value generated by the caller for 
           this call.
            It is replied back to the LAN for the caller to recognize a 
            reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated 
            to it.
           """
        if self.me == callee_nip and \
           self.radar.neigh.netid == callee_netid:
            ret = None
            rpc.UDP_send_keepalive_forever_start(_rpc_caller, caller_id)
            try:
                logging.log(logging.ULTRADEBUG, 'calling free_nodes_nb...')
                ret = self.free_nodes_nb(lvl)
                logging.log(logging.ULTRADEBUG, 'returning ' + str(ret))
            except Exception as e:
                ret = ('rmt_error', e.message)
                logging.warning('free_nodes_nb_udp: returning exception ' + 
                                str(ret))
            finally:
                rpc.UDP_send_keepalive_forever_stop(caller_id)
            logging.log(logging.ULTRADEBUG, 'calling UDP_send_reply...')
            rpc.UDP_send_reply(_rpc_caller, caller_id, ret)

    def route_add(self, lvl, dst, gw, rem_at_gw, hops, silent=0):
        ''' Add a new route '''

        logging.log(logging.ULTRADEBUG, 'maproute.route_add')
        # If destination is me I won't add a route.
        if self.me[lvl] == dst:
            logging.debug('I won\'t add a route to myself (%s, %s).' % 
                          (lvl, dst))
            logging.debug(get_stackframes(back=1))
            return 0

        n = self.node_get(lvl, dst)
        was_free = n.is_free()
        ret, oldrem_at_gw = n.route_add(lvl, dst, gw, rem_at_gw, hops)
        if was_free and not n.is_free():
            # The node is new
            self.node_add(lvl, dst)
        if not silent:
            if ret == 1:
                self.events.send('ROUTE_NEW', (lvl, dst, gw.id, rem_at_gw))
            elif ret == 2:
                self.events.send('ROUTE_REM_CHGED', (lvl, dst, gw.id, 
                                                     rem_at_gw, oldrem_at_gw))
        return ret

    def route_del(self, lvl, dst, gw_id, gwip, silent=0):

        logging.log(logging.ULTRADEBUG, 'maproute.route_del')
        # If destination is me I won't delete a route. Pretend it didn't happen.
        if self.me[lvl] == dst:
            logging.debug('I won\'t delete a route to myself (%s, %s).' % 
                          (lvl, dst))
            logging.debug(get_stackframes(back=1))
            return 0

        d = self.node_get(lvl, dst)
        d.route_del(gw_id)

        if d.is_empty():
            # No more routes to reach the node (lvl, dst).
            # Consider it dead
            self.node_del(lvl, dst)

        if not silent:
            self.events.send('ROUTE_DELETED', (lvl, dst, gwip))

        return 1

    def route_rem(self, lvl, dst, gw_id, newrem_at_gw, new_hops, silent=0):
        """Changes the rem_at_gw and hops of the route with gateway `gw'

        Returns 0 if the route doesn't exists, 1 else."""

        logging.log(logging.ULTRADEBUG, 'maproute.route_rem')
        # If destination is me I won't do a route change. Pretend it didn't happen.
        if self.me[lvl] == dst:
            logging.debug('I won\'t update a route to myself (%s, %s).' % 
                          (lvl, dst))
            logging.debug(get_stackframes(back=1))
            return 0

        d = self.node_get(lvl, dst)
        ret, val = d.route_rem(gw_id, newrem_at_gw, new_hops)
        if ret:
            oldrem_at_gw = val
            if not silent:
                self.events.send('ROUTE_REM_CHGED',
                                 (lvl, dst, gw_id, newrem_at_gw, 
                                  oldrem_at_gw))
            return 1
        else:
            return 0

    def route_change(self, lvl, dst, gw, rem_at_gw, hops):
        """Changes routes according to parameters obtained in a ETP.
        
        Returns 1 if the ETP is interesting, 0 otherwise."""

        # The method has to handle several cases:
        # 1. We may have or may not have a current route through this gw.
        # 2. Argument rem_at_gw may be a DeadRem instance.
        #    It means that the sender of the ETP, that is gw, has no routes
        #    (maybe except directly through us) to reach this destination.
        #    So we want the route through gw to be deleted in our map.
        # 3. Argument rem_at_gw may be a valid REM, but then hops may
        #    contain our ip. In this case if we created the instance of our
        #    Route (or updated the existing one's Neigh) we would discover 
        #    that its rem property is a DeadRem.
        #    It means that the sender of the ETP, that is gw, excluding the
        #    route passing *directly* through us, has another best-route...
        #       WARNING: We don't know if gw has also other routes to dst!
        #                Could this be a problem? TODO answer.
        #    ... and that route passes indirectly through us.
        #    So we can't use gw to reach dst. Were we previuosly able to?
        #    If we were, then we want the route through gw to be deleted in
        #    our map, and that information to be forwarded.
        #    If we were not, then we want to remove dst from the routes
        #    in the processed ETP. We inform about this by returning 0.
        
        logging.log(logging.ULTRADEBUG, 'maproute.route_change')
        # If destination is me I won't do a route change. Pretend it didn't 
        # happen.
        if self.me[lvl] == dst:
            logging.debug('I won\'t update a route to myself (%s, %s).' % 
                          (lvl, dst))
            logging.debug(get_stackframes(back=1))
            return 0

        # Do we have this route?
        d = self.node_get(lvl, dst)
        r = d.route_getby_gw(gw.id)
        was_there = r is not None

        if isinstance(rem_at_gw, DeadRem):
            # We want the route deleted (if present)
            ret = 0
            if was_there:
                ret = self.route_del(lvl, dst, gw.id, gw.ip)
                if ret: logging.debug('    Deleted old route.')
            return ret
        elif self.nip_to_ip(self.me) in hops:
            # We want the route deleted (if present)
            if was_there:
                ret = self.route_del(lvl, dst, gw.id, gw.ip)
                if ret: logging.debug('    Deleted old route.')
                return ret
            else:
                # else, don't forward this part of R
                return 0
        else:
            # We want the route added or updated
            if was_there:
                ret = self.route_rem(lvl, dst, gw.id, rem_at_gw, hops)
                if ret: logging.debug('    Updated old route.')
            else:
                ret = self.route_add(lvl, dst, gw, rem_at_gw, hops)
                if ret: logging.debug('    Added new route.')
            return ret

    def route_reset(self, lvl, dst):
        # I must delete all the routes in the map and in the kernel
        gwip = None
        node = self.node_get(lvl, dst)
        while not node.is_free():
            # starting from the worst
            gw = node.routes[-1].gw
            gwip = gw.ip
            node.route_del(gw.id)
        # No more routes to reach the node (lvl, dst).
        # Consider it dead, if it wasn't already.
        self.node_del(lvl, dst)
        if gwip:
            # Just one event should be enough.
            self.events.send('ROUTE_DELETED', (lvl, dst, gwip))

    def map_data_pack(self):
        """Prepares a packed_maproute to be passed to maproute.map_data_merge
        in another host."""
        logging.error('MapRoute must not be replicated that way.')
        raise NotImplementedError

    def map_data_merge(self, (nip, plist, nblist)):
        """Copies a maproute from another nip's point of view."""
        logging.error('MapRoute must not be replicated that way.')
        raise NotImplementedError

    def repr_me(self, func_repr_node=None):
        def repr_node_maproute(node):
            coding_gw = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
            free_repr = '  '
            if node.is_free(): return free_repr
            str_repr_node = ''
            for r in node.routes:
                if len(str_repr_node) > 0: str_repr_node += ','
                str_repr_node += coding_gw[r.gw.id]
                str_repr_node += '(' + str(r.rem.value) + '#' + \
                               str(len(r.hops)) + ')'
            if node.its_me: str_repr_node = 'ITSME'
            if len(str_repr_node) < len(free_repr):
                padding = len(free_repr) - len(str_repr_node)
                str_repr_node += free_repr[:padding]
            return str_repr_node
        if func_repr_node is None: func_repr_node = repr_node_maproute
        return Map.repr_me(self, func_repr_node)

## Neighbour stuff

    def routeneigh_del(self, neigh):
        """Delete from the MapRoute all the routes passing from the
           gateway `neigh.id' and delete the node `neigh' itself 
           (if present)"""

        for lvl in xrange(self.levels):
            for dst in xrange(self.gsize):
                # Don't try deleting a route towards myself.
                if self.me[lvl] != dst:
                    node = self.node_get(lvl, dst)
                    if not node.is_free():
                        if node.route_getby_gw(neigh.id) is not None:
                            self.route_del(lvl, dst, neigh.id, neigh.ip)
        logging.debug('ANNOUNCE: gw ' + str(neigh.id) + ' removable.')
        self.radar.neigh.announce_gw_removable(neigh.id)

    def routeneigh_rem(self, neigh, oldrem):
        for lvl in xrange(self.levels):
            for dst in xrange(self.gsize):
                # Don't try changing a route towards myself.
                if self.me[lvl] != dst:
                    node = self.node_get(lvl, dst)
                    if not node.is_free():
                        r = node.route_getby_gw(neigh.id)
                        if r is not None:
                            r.update_gw(neigh)

    def routeneigh_get(self, neigh):
        """Converts a neighbour to a (g)node of the map"""
        neigh.nip = self.ip_to_nip(neigh.ip)   # TODO: refactoring Neigh
        lvl = self.nip_cmp(self.me, neigh.nip)
        return (lvl, neigh.nip[lvl])

    def bestroutes_get(self, f=ftrue, exclude_gw_ids=[]):
        """Returns the list of all the best routes of the map.

           Let L be the returned list, then L[lvl] is the list of all the best
           routes of level lvl of the map. An element of this latter list is a
           tuple (dst, gw, rem, hops), where:
            dst is the destination of the route;
            gw is its gateway as an instance of Neigh;
            rem is the REM from us to destination (it's not rem_at_gw);
            hops is the sequence of hops from us to destination (it includes 
            our ip).

           If a function `f' has been specified, then each element L[lvl][i]
           in L is such that f(L[lvl][i])==True.
           
           If a list of gateway IDs have been specified in exclude_gw then the
           routes using those gateways are not considered when searching for
           best routes.
           """
        return [
                [ (dst, br.gw, br.rem, br.hops+[(0, self.me[0])])
                    for dst in xrange(self.gsize)
                        for br in [self.node_get(lvl, dst)
                                   .best_route(exclude_gw_ids=exclude_gw_ids)]
                                if br is not None and f((dst, br.gw, br.rem, 
                                        br.hops+[(0, self.me[0])]))
                ] for lvl in xrange(self.levels)
               ]
