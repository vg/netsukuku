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
# Setting up the ntk iproute table

import re

RT_TABLE = '/etc/iproute2/rt_tables'
NTK_TABLE = 26
NTK_IN_RT_TABLE = re.compile(r'\d+\s+ntk')

def update_rt_table():
    conf = ''
    rt_table = open(RT_TABLE, 'a+')
    try:
        rt_table_content = rt_table.read()
        if not NTK_IN_RT_TABLE.search(rt_table_content):
            if not rt_table_content[-1] == '\n':
                conf += '\n'
            conf += '# Added by netsukuku\n'
            conf += '%s\tntk\n' % NTK_TABLE
            rt_table.write(conf)
    finally:
        rt_table.close()

update_rt_table()
