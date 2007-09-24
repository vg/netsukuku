#!/usr/bin/env python
##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
#
# Netsukuku distutils installer
#

import sys
if sys.version_info < (2,5):
    print 'You must use at least Python 2.5 for Netsukuku' # TODO: check more deeply
    sys.exit(3)

from distutils.core import setup
from config import VERSION

setup(name='pyntk', # or python-netsukuku?
    description='Mesh network that generates and sustains itself autonomously.',
    long_description=\
'''
Netsukuku is a mesh network or a p2p net system that generates and sustains
itself autonomously. It is designed to handle an unlimited number of nodes
with minimal CPU and memory resources. Thanks to this feature it can be
easily used to build a worldwide distributed, anonymous and anarchical
network, separated from the Internet, without the support of any servers,
ISPs or control authorities.
'''
    author='The Netsukuku Team',
    author_email='', # Create a shared project mail?
    url='http://www.netsukuku.org',
    version=VERSION,
    license='General Public License',
    packages=['core', 'lib', 'network'])