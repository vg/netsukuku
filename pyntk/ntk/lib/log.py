##
# This file is part of Netsukuku
# (c) Copyright 2009 Daniele Tricoli aka Eriol <eriol@mornie.org>
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

import logging
import logging.handlers
import os.path
import sys

from ntk.config import settings

LOG_FILE = os.path.join(settings.LOG_DIR, settings.LOG_FILE)

def config():
    ''' Configure the logging system using `settings'. '''

    if settings.DEBUG:
        m = ('%(asctime)s %(levelname)s:'
             '(%(filename)s at line %(lineno)d): %(message)s')
    else:
        m = '%(asctime)s %(levelname)s: %(message)s'

    formatter = logging.Formatter(m)

    rfh = logging.handlers.RotatingFileHandler(LOG_FILE,
                                               maxBytes=1024 * 20,
                                               backupCount=8)
    rfh.setFormatter(formatter)

    console = logging.StreamHandler()
    console.setFormatter(formatter)

    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)
    logger.addHandler(rfh)

    if settings.DEBUG:
        logger.addHandler(console)

    return logger

logger = config()

def get_stackframes(back=0):
    ret = sys._current_frames().items()[0][1]
    while not ret is None and ret.f_back and back >= 0:
        ret = ret.f_back
        back -= 1
    return get_stackframes_repr(ret)

def get_stackframes_repr(frame):
    ret = []
    while True:
        ret.append((frame.f_code.co_filename, frame.f_code.co_name, frame.f_lineno))
        frame = frame.f_back
        if not frame: break
    return ret.__repr__()

