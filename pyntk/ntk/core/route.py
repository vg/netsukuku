##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# (c) Copyright 2009 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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
from ntk.core.status import ZombieException

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

        if isinstance(b, DeadRem):
            if isinstance(self , DeadRem):
                return 0
            else:
                return 1
        if isinstance(b, NullRem):
            if isinstance(self , NullRem):
                return 0
            else:
                return -1
        if isinstance(self, DeadRem): return -1
        if isinstance(self, NullRem): return 1
        if self.value < b.value: return 1
        if self.value > b.value: return -1
        return 0

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

        if isinstance(b, DeadRem): return 1
        if isinstance(b, NullRem): return -1
        if self.value < b.value: return 1
        if self.value > b.value: return -1
        return 0

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
    __slots__ = ['routenode', 'gw', 'rem_at_gw', 'hops', 'rem']

    def __init__(self, gw, rem_at_gw, hops, routenode):
        """New gw route"""
        self.gw = gw
        self.rem_at_gw = rem_at_gw
        self.hops = hops
        self.routenode = routenode

    def _get_rem(self):
        return self.rem_at_gw + self.gw.rem

    rem = property(_get_rem)

    def _get_hops_with_gw(self):
        if self.first_hop == self.dest:
            return []
        else:
            return [self.first_hop] + self.hops

    # this contains gateway, but not destination in any case.
    hops_with_gw = property(_get_hops_with_gw)

    def _get_first_hop(self):
        return self.routenode.maproute.routeneigh_get(self.gw)

    first_hop = property(_get_first_hop)

    def _get_dest(self):
        return self.routenode.lvl, self.routenode.id

    dest = property(_get_dest)

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
            return self.gw is b.gw
        else:
            return False

    def __ne__(self, other):
        return not self.__eq__(other)

    def contains(self, hop):
        # hop is a pair (lvl, id)
        if hop == self.dest:
            return True
        if hop == self.first_hop:
            return True
        # If the passed neighbour is in the middle of this path...
        if hop in self.hops:
            return True
        # Otherwise the path does not contain the passed neighbour
        return False

    def rem_modify(self, newrem_at_gw, new_hops):
        """Sets new rem_at_gw and hops and returns the old rem_at_gw"""
        oldrem_at_gw = self.rem_at_gw
        self.rem_at_gw = newrem_at_gw
        self.hops = new_hops
        return oldrem_at_gw

    def __repr__(self):
        return '<Route: gw(%s), rem(%s), #hops(%s)>' % (self.gw.ip, self.rem,
                                                        len(self.hops))

class RouteNode(object):
    """List of paths to a known destination.

    This class is basically a list of Route instances, where the
    destination node and its level are fixed and known.

    Note: for each gateway G there's only one route in self.routes,
          which has the same gateway G
    """

    __slots__ = ['maproute', 'routes', 'busy', 'lvl', 'id', 'its_me']

    def __init__(self, maproute, lvl, id, its_me=False):
        self.lvl = lvl
        self.id = id
        self.its_me = its_me
        self.busy = False
        self.routes = []
        self.maproute = maproute

    def route_getby_gw_neigh(self, gw):
        """Returns the route having as gateway `gw'"""
        for r in self.routes:
            if r.gw is gw:
                return r
        return None

    def update_route_by_gw(self, nr, rem_at_gw, hops):
        r = self.route_getby_gw_neigh(nr)
        if r is None:
            self.routes.append(Route(nr, rem_at_gw, hops, self))
        else:
            oldrem_at_gw = r.rem_modify(rem_at_gw, hops)
        self.busy = True # For sure now we are busy.
        self.sort()

    def route_del_by_neigh(self, gw):
        """Delete a route.

        Returns 1 if the route has been deleted, otherwise 0"""

        r = self.route_getby_gw_neigh(gw)
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
        return not self.busy

    def nroutes(self):
        return len(self.routes)

    def best_route(self):
        self.sort()
        if any(self.routes):
            return self.routes[0]
        else:
            return None

    def best_route_without(self, hop):
        self.sort()
        for r in self.routes:
            if not r.contains(hop): return r
        return None

    def __repr__(self):
        return '<RouteNode: %s>' % self.routes

def ftrue(*args):
    return True

class MapRoute(Map):
    """Map of routes, all of a same Rem type.

    MapRoute.node[lvl][id] is a RouteNode class, i.e. a list of routes
    having as destination the node (lvl, id)"""

    __slots__ = Map.__slots__ + ['ntkd_status', 'radar', 'remotable_funcs']

    def __init__(self, ntkd_status, levels, gsize, me):

        self.ntkd_status = ntkd_status
        self.radar = None
        Map.__init__(self, levels, gsize, RouteNode, me)

        self.events.add(['ROUTES_UPDATED', 'ROUTES_RESET'])
        self.remotable_funcs = [self.free_nodes_nb, self.free_nodes_nb_udp]

    def set_radar(self, radar):
        self.radar = radar

    ## Methods that modify routes
    ##

    def update_route_by_gw(self, dest, nr, rem_at_gw, hops):
        # dest is a pair (lvl, id)
        # nr is a instance of Neigh

        # If dest is me, strange thing. TODO review.
        lvl, dst = dest
        if self.me[lvl] == dst:
            return

        paths = self.node_get(lvl, dst)
        was_free = paths.is_free()
        if isinstance(rem_at_gw, DeadRem):
            paths.route_del_by_neigh(nr)
        else:
            paths.update_route_by_gw(nr, rem_at_gw, hops)

        if was_free and not paths.is_free():
            # The node is new
            self.node_add(lvl, dst)
        if paths.is_empty():
            # No more routes to reach the node (lvl, dst).
            # Consider it dead
            self.node_del(lvl, dst)

        self._emit_routes_updated(lvl, dst)

    def route_del_by_neigh(self, lvl, dst, gw, gwip, silent=0):

        logging.log(logging.ULTRADEBUG, 'maproute.route_del_by_neigh')
        # If destination is me I won't delete a route. Pretend it didn't happen.
        if self.me[lvl] == dst:
            logging.debug('I won\'t delete a route to myself (%s, %s).' % 
                          (lvl, dst))
            logging.debug(get_stackframes(back=1))
            return

        d = self.node_get(lvl, dst)
        d.route_del_by_neigh(gw)

        if d.is_empty():
            # No more routes to reach the node (lvl, dst).
            # Consider it dead
            self.node_del(lvl, dst)

        if not silent:
            self._emit_routes_updated(lvl, dst)

    def route_signal_rem_changed(self, lvl, dst):
        self._emit_routes_updated(lvl, dst)

    def _emit_routes_updated(self, lvl, dst):
        self.events.send('ROUTES_UPDATED', (lvl, dst))

    def map_reset(self):
        Map.map_reset(self)
        self.events.send('ROUTES_RESET', ())

    ######

    def call_free_nodes_nb_udp(self, neigh, lvl):
        """Use BcastClient to call free_nodes_nb"""
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

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if self.me == callee_nip and \
           self.radar.neigh.netid == callee_netid:
            ret = None
            rpc.UDP_send_keepalive_forever_start(_rpc_caller, caller_id)
            try:
                logging.log(logging.ULTRADEBUG, 'calling free_nodes_nb...')
                ret = self.free_nodes_nb(lvl)
                logging.log(logging.ULTRADEBUG, 'returning ' + str(ret))
            except Exception as e:
                ret = ('rmt_error', str(e))
                logging.warning('free_nodes_nb_udp: returning exception ' + 
                                str(ret))
            finally:
                rpc.UDP_send_keepalive_forever_stop(caller_id)
            logging.log(logging.ULTRADEBUG, 'calling UDP_send_reply...')
            rpc.UDP_send_reply(_rpc_caller, caller_id, ret)

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
            free_repr = '  '
            if node.is_free(): return free_repr
            str_repr_node = ''
            for r in node.routes:
                if len(str_repr_node) > 0: str_repr_node += ','
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
           gateway `neigh' and delete the node `neigh' itself 
           (if present)"""

        for lvl in xrange(self.levels):
            for dst in xrange(self.gsize):
                # Don't try deleting a route towards myself.
                if self.me[lvl] != dst:
                    node = self.node_get(lvl, dst)
                    if not node.is_free():
                        if node.route_getby_gw_neigh(neigh) is not None:
                            self.route_del_by_neigh(lvl, dst, neigh, neigh.ip)
        logging.debug('ANNOUNCE: gw ' + str((neigh.ip, neigh.netid)) + ' removable.')
        self.radar.neigh.announce_gw_removable((neigh.ip, neigh.netid))

    def routeneigh_get(self, neigh):
        """Converts a neighbour to a (g)node of the map"""
        neigh_nip = self.ip_to_nip(neigh.ip)
        lvl = self.nip_cmp(self.me, neigh_nip)
        return (lvl, neigh_nip[lvl])

## Helpers

    def choose_between(self, choose_from, get_nip=None, get_preference=None, \
            exclude_hop=None, casuality_coeff=1, preference_coeff=1, \
            rem_coeff=1, proximity_coeff=1):
        '''Given a set of objects, each one associated to a nip, choose one based on various criteria
           Possible criteria:
            . casuality
            . weight (preference)
            . rem to reach the g-node
            . proximity (nearest in NIP)
        '''
        # An object is returned that was in the set "choose_from".
        # An object in the set, passed to the function "get_nip", returns a NIP.
        # We use lvl=self.nip_cmp to get the first level different from us. Then we use
        # the position (lvl, nip[lvl]) in our Map.
        # So, the NIP returned by "get_nip" has to have valid id from "levels" to "lvl"
        # and the remaining id could be None.
        from random import randint

        def equality(x):
            return x
        if get_nip is None:
            get_nip = equality

        def no_preference(x):
            return 1
        if get_preference is None:
            get_preference = no_preference

        def get_best_route(rtnode):
            if exclude_hop is None:
                return rtnode.best_route()
            else:
                return rtnode.best_route_without(exclude_hop)

        def distance(a,b):
            x = abs(a-b)
            if x > self.gsize/2: x = self.gsize-x
            return x

        def proximity(lvl, pos):
            return 1 / (distance(self.me[lvl], pos) * self.gsize ** lvl)

        ret = None
        casuality_wg = []
        preference_wg = []
        rem_wg = []
        proximity_wg = []
        for i in xrange(len(choose_from)):
            obj = choose_from[i]
            nip = get_nip(obj)
            preference = get_preference(obj)
            lvl = self.nip_cmp(nip)
            pos = nip[lvl]
            rtnode = self.node_get(lvl, pos)
            if rtnode.is_free():
                # of course we cannot choose this one
                casuality_wg.append(0)
                preference_wg.append(0)
                rem_wg.append(0)
                proximity_wg.append(0)
                continue
            rt = get_best_route(rtnode)
            if rt is None or isinstance(rt.rem, DeadRem):
                # of course we cannot choose this one
                casuality_wg.append(0)
                preference_wg.append(0)
                rem_wg.append(0)
                proximity_wg.append(0)
                continue
            casuality_wg.append(1)
            preference_wg.append(get_preference(obj))
            proximity_wg.append(proximity(lvl, pos))
            rem = rt.rem
            if isinstance(rem, Rtt):
                rem_wg.append(1 / rem.value)
            else:
                raise Exception, 'Not implemented for ' + repr(rem)

        casuality_sum = sum(casuality_wg)
        preference_sum = sum(preference_wg)
        rem_sum = sum(rem_wg)
        proximity_sum = sum(proximity_wg)
        max_sum = max(casuality_sum, preference_sum, rem_sum, proximity_sum)
        value = 0
        values = []
        for i in xrange(len(choose_from)):
            if casuality_wg[i] > 0:
                value += casuality_wg[i] * max_sum / casuality_sum * casuality_coeff
            if preference_wg[i] > 0:
                value += preference_wg[i] * max_sum / preference_sum * preference_coeff
            if rem_wg[i] > 0:
                value += rem_wg[i] * max_sum / rem_sum * rem_coeff
            if proximity_wg[i] > 0:
                value += proximity_wg[i] * max_sum / proximity_sum * proximity_coeff
            values.append(value)
        # choose
        if value == 0: return None
        choi = randint(1, value)
        for i in xrange(len(choose_from)):
            if values[i] >= choi:
                ret = choose_from[i]
                break

        return ret

    def choose_fast(self, choose_from, get_nip=None, exclude_hop=None):
        return self.choose_between(choose_from, get_nip=get_nip, get_preference=None, \
            exclude_hop=exclude_hop, casuality_coeff=0, preference_coeff=0, \
            rem_coeff=1, proximity_coeff=0)

