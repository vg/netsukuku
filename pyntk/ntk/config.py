##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
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

import imp
import os
import sys

# Settings handled with a read only property
NOT_OVERRIDABLE_SETTINGS = ('LEVELS', 'BITS_PER_LEVEL')

DEFAULT_SETTINGS = dict(
    # Inet
    IP_VERSION = 4,
    # Radar
    MAX_BOUQUET = 16,
    MAX_NEIGH = 16,
    MAX_WAIT_TIME = 8, # seconds
    MULTIPATH = False,
    SIMULATED = False,
)

if sys.platform == 'linux2':
    GLOBAL_SETTINGS = dict(
        CONFIGURATION_DIR = '/etc/netsukuku',
        CONFIGURATION_FILE = 'settings.conf',
        DATA_DIR = '/usr/share/netsukuku',
        PID_DIR  = '/var/run',
    )
else:
    raise Exception('Your platform is not supported yet.')

GLOBAL_SETTINGS.update(DEFAULT_SETTINGS)

class ImproperlyConfigured(Exception):
    ''' Improperly configured error '''

class Settings(object):

    def __init__(self):
        for setting in GLOBAL_SETTINGS:
            # Configuration settings must be uppercase
            if setting == setting.upper():
                setattr(self, setting, GLOBAL_SETTINGS[setting])

        self.load_configuration_file()

    def load_configuration_file(self):
        configuration_file = os.path.join(self.CONFIGURATION_DIR,
                                          self.CONFIGURATION_FILE)

        if not os.path.isfile(configuration_file):
            # No configuration file. Ignore it.
            return

        try:
            user_settings = imp.load_source('configuration_file',
                                            configuration_file)
        except IOError, e:
            raise IOError("Could not load '%s': %s" % (configuration_file, e))

        except SyntaxError, e:
            raise SyntaxError("Error in '%s': %s" % (configuration_file, e))

        for setting in dir(user_settings):
            if setting == setting.upper():
                if setting not in NOT_OVERRIDABLE_SETTINGS:
                    setting_value = getattr(user_settings, setting)
                    setattr(self, setting, setting_value)

    def _get_levels(self):
        ''' Returns proper LEVELS according IP_VERSION '''
        if self.IP_VERSION == 4:
            return 4
        elif self.IP_VERSION == 6:
            return 16
        else:
            raise ImproperlyConfigured('IP_VERSION must be either 4 or 6')

    LEVELS = property(_get_levels)

    def _get_bits_per_level(self):
        ''' Returns proper bits according IP_VERSION and LEVELS '''
        # IP_VERSION_BIT = {4: 32, 6: 128}
        return 8 # ---> IP_VERSION_BIT[self.IP_VERSION] / self.LEVELS

    BITS_PER_LEVEL = property(_get_bits_per_level)

settings = Settings()
