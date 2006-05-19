<?php  

$about = <<<EOF
<pre id="filez">
  *  What is this?
  
  *  Get the code!
  
  *  Build and install
  
  *  Kernel dependencies
  
  *  How to use it
  
  *  Where to get in touch with us
  
  *  Bug report
  
  *  Hack the code
  
  *  License and that kind of stuff...
  
--


**
****  What is this?
**

Netsukuku is a mesh network or a p2p net system that generates and sustains
itself autonomously. It is designed to handle an unlimited number of nodes
with minimal CPU and memory resources. Thanks to this feature it can be easily
used to build a worldwide distributed, anonymous and anarchical network,
separated from the Internet, without the support of any servers, ISPs or
authority controls.
This net is composed by computers linked physically each other, therefore it
isn't build upon any existing network. Netsukuku builds only the routes which
connects all the computers of the net.
In other words, Netsukuku replaces the level 3 of the model iso/osi with
another routing protocol.
The Domain Name System is also replaced by a decentralised and distributed
system: the Abnormal Netsukuku Domain Name Anarchy.

The complete features list of Netsukuku is here:
<a href="http://netsukuku.freaknet.org/files/doc/misc/Ntk_features_list">http://netsukuku.freaknet.org/files/doc/misc/Ntk_features_list</a>


In order to join to Netsukuku you have to use NetsukukuD, which is the daemon
implenting the Npv7 protocol.

Before doing anything, please read the documentation in doc/ or in
<a href="http://netsukuku.freaknet.org">http://netsukuku.freaknet.org</a>


**
****  Get the code!
**

Get the tarball of the latest stable version from:
<a href="http://netsukuku.freaknet.org/files/">http://netsukuku.freaknet.org/files/</a>


If you want to download the development code you have to checkout it from the
cvs repository:
(Warning: It is highly probable the development code will not work!)

$ cvs -d :pserver:anoncvs@hinezumilabs.org:/home/cvsroot login
or
$ export CVSROOT=":pserver:anoncvs@hinezumilabs.org:/home/cvsroot"
$ cvs login

then check it out:

$ cvs -z3 -d :pserver:anoncvs@hinezumilabs.org:/home/cvsroot co netsukuku
or
$ cvs -z3 co netsukuku
(providing the CVSROOT variable was set in the previous step)


Once you've checked out a copy of the source tree, you can update 
your source tree at any time so it is in sync with the latest and 
greatest by running the command:
# cvs -z3 update -d -P


**
****  Build and install
**

To compile the code you can use scons or just go with the old school way:

# ./configure && make && make install

But SCons is cooler:
<a href="http://www.scons.org">http://www.scons.org/</a>
(You should have installed at least the 2.4 version of Python in order to
avoid dirty bugs in scons)


The code depends also on the libgmp and openssl. Generally you have
already them installed on your system, but eventually you can retrieve them
here: 
for the libgmp:			<a href="http://www.swox.com/gmp/">http://www.swox.com/gmp/</a>
the openssl library here:	<a href="http://openssl.org">http://openssl.org</a>

Then go in the src/ directory and type:
$ scons --help

That will show you all the options you can use in the build and installation
process. Finally execute:

$ scons 

The code will be compiled. If all went well install NetsukukuD with:

# scons install

Now you should give a look at /etc/netsukuku.conf (or wherever you installed
it) and modify it for your needs, but generally the default options are good.

- Notes:

If you want to change some scons option to do another installation, (i.e. you
may want to reinstall it with another MANDIR path), you have to run:
$ scons --clean

**
****  Kernel dependencies
**

On Linux be sure to have the following options set in your kernel .config.
These options are taken from linux-2.6.14.
 
	#
	# Networking options
	#
	CONFIG_PACKET=y
	CONFIG_UNIX=y
	CONFIG_INET=y
	CONFIG_IP_MULTICAST=y
	CONFIG_IP_ADVANCED_ROUTER=y
	CONFIG_IP_MULTIPLE_TABLES=y
	CONFIG_IP_ROUTE_MULTIPATH=y
	CONFIG_NET_IPIP=y
	CONFIG_NETFILTER=y

	#
	# IP: Netfilter Configuration
	#
	CONFIG_IP_NF_CONNTRACK=y
	CONFIG_IP_NF_FTP=y
	CONFIG_IP_NF_IPTABLES=y
	CONFIG_IP_NF_FILTER=y
	CONFIG_IP_NF_TARGET_REJECT=y
	CONFIG_IP_NF_NAT=y
	CONFIG_IP_NF_NAT_NEEDED=y
	CONFIG_IP_NF_TARGET_MASQUERADE=y
	CONFIG_IP_NF_NAT_FTP=y

If you are using modules you have to load them before launching the daemon.


**
****  How to use it
**

Before doing anything do:

$ man netsukuku_d
$ man andna

when you feel confortable and you are ready to dare type with root
priviledges:

# netsukuku_d

then just wait... ^_-

(For the first times it's cool to use the -D option to see what happens).

-  Note:
The daemon at startup takes the list of all the network interfaces which are
currently UP and it uses all of them to send and receive packets. If you want
to force the daemon to use specific interfaces you should use the B<-i>
option.


**
****  Where to get in touch with us
**

Subscrive to the netsukuku mailing to get help, be updated on the latest news
and discuss on its development.

To subscribe to the list, send a message to:
   netsukuku-subscribe@freaknet.org
   
You can browse the archive here:
<a href="http://dir.gmane.org/gmane.network.peer-to-peer.netsukuku">http://dir.gmane.org/gmane.network.peer-to-peer.netsukuku</a>
   
We live night and day in IRC, come to see us in:

#netsukuku 
on the FreeNode irc server (irc.freenode.org).


**
****  Bug report
**

{ Don't panic! }

If you encounter any bug, please report it to netsukuku@freaknet.org or
contact any author explaining what the problem is and if possible a way to
reproduce it.


**
****  Hack the code
**

Feel free to debug, patch, modify and eat the code. Then submit your results
to the mailing list ^_-

There is a lot to code too! If you are a Kung Foo coder, get on board and
help the development writing some nice poems. For a start you can take a look
at the src/TODO file.


**
****  License and that kind of stuff...
**

All the Netsukuku code is released under the GPL-2, please see the COPYING
file for more information.

The authors of Netsukuku and NetsukukuD are listed in the file AUTHORS.
</pre>
EOF;

print $about;

?>
