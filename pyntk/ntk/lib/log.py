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
import traceback

from ntk.config import settings

LOG_FILE = os.path.join(settings.LOG_DIR, settings.LOG_FILE)

logger = logging.getLogger('')

def init_logger():
    ''' Configure the logging system using `settings'. '''

    global logger
    if settings.VERBOSE_LEVEL > 0:
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

    logger.ULTRADEBUG = 5
    logging.addLevelName(logger.ULTRADEBUG, 'ULTRADEBUG')
    if settings.VERBOSE_LEVEL > 4: settings.VERBOSE_LEVEL = 4
    levels = {0 : logging.ERROR, 1 : logging.WARNING, 2 : logging.INFO, 
              3 : logging.DEBUG, 4 : logger.ULTRADEBUG}
    logger.setLevel(levels[settings.VERBOSE_LEVEL])
    logger.addHandler(rfh)

    if settings.DEBUG_ON_SCREEN:
        logger.addHandler(console)

def get_stackframes(back=0):
    ret = sys._current_frames().items()[0][1]
    while ret is not None and ret.f_back and back >= 0:
        ret = ret.f_back
        back -= 1
    return get_stackframes_repr(ret)

def get_stackframes_repr(frame):
    ret = []
    while True:
        ret.append((frame.f_code.co_filename, frame.f_code.co_name, 
                    frame.f_lineno))
        frame = frame.f_back
        if not frame: break
    return ret.__repr__()

def log_exception_stacktrace(e, indent=2):
    spaces = ' ' * indent
    excinfo = sys.exc_info()
    tb = excinfo[2]
    logger.error(spaces + "Exception: %s" % (e.__repr__(), ))
    logger.error(spaces + "Stacktrace:")
    frames = traceback.extract_tb(tb)
    for fr in frames:
        logger.error(spaces + "  File \"%s\", line %s, in %s" % (fr[0], 
                                                                 fr[1], 
                                                                 fr[2]))
        logger.error(spaces + "    %s" % (fr[3], ))

def log_on_file(filename, msg):
    f = None
    try:
        f = open(filename, 'a')
    except:
        return
    try:
        f.write(str(msg) + '\n')
    finally:
        f.close()

logger.log_on_file = log_on_file

