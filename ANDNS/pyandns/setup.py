from distutils.core import setup, Extension

__version__= "0.0.1"
__desc__= \
""" This module, andns, is the implementation of andns protocol for python.
To be more precise, this module is simply a wrapper to the andns C lib.
The andns protocol was born inside netsukuku (www.netsukuku.org)."""

_andns_ext= Extension("andns._andns", sources= ["andns/_andns.c"], libraries = ['andns'])

setup(name='andns',
    version= __version__,
    author='Federico Tomassini aka efphe',
    author_email='efphe@freaknet.org',
    maintainer= 'Federico Tomassini aka efphe',
    maintainer_email='efphe@freaknet.org',
    url='http://www.netsukuku.org',
    packages=['andns'],
    description= __desc__,
    license= "BSD",
    platforms = ["unix"],
    ext_modules= [_andns_ext]
)
