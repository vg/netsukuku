<?php

$contacts = <<<EOF
<pre id="filez">

**
***  General contacts
**

Subcrive to the netsukuku mailing to get help, be updated on the latest news
and discuss on its development.

To subscribe to the list, send a message to:
   netsukuku-subscribe@freaknet.org

You can browse the archive here:
<a href="http://dir.gmane.org/gmane.network.peer-to-peer.netsukuku">http://dir.gmane.org/gmane.network.peer-to-peer.netsukuku</a>

We live night and day in IRC, come to see us in:

#netsukuku
on the FreeNode irc server (irc.freenode.org).


**
***  Authors
**

Main authors and maintainers:

Andrea Lo Pumo aka AlpT &lt;alpt@netsukuku.org&gt;


Contributors (in chronological order of initial contribution):

Andrea Leofreddi &lt;andrea.leofreddi@gmail.com&gt; wrote the DNS wrapper code
(dns_pkt.cpp, dns_rendian.h, dns_utils.h).

Enzo Nicosia (Katolaz) &lt;katolaz@netsukuku.org&gt; wrote the files needed to use
Automake/Autoconf. (Makefile.am configure.ac, src/Makefile.am,
src/man/Makefile.am).

Federico Tomassini (Efphe) &lt;efphe@netsukuku.org&gt; wrote andns, which provides
DNS compatibility for the ANDNA system. He also wrote the interface to the
libiptc. (andns* dnslib.[ch] mark.[ch] libiptc/*)


Website developers and maintainers:

Crash           &lt;crash@netsukuku.org&gt;,
Entropika       &lt;e@entropika.net&gt;,
Black           &lt;black@netsukuku.org&gt;


**
*** Related links
**

 - Wiki      : <a href="http://lab.dyne.org/Netsukuku">http://lab.dyne.org/Netsukuku</a>
 - Cvs online: <a href="http://www.hinezumilabs.org/viewcvs/netsukuku/">http://www.hinezumilabs.org/viewcvs/netsukuku/</a>

 - <a href="http://www.freaknet.org">http://www.freaknet.org</a>
 - <a href="http://www.dyne.org">http://www.dyne.org</a>
 - <a href="http://www.hinezumilabs.org">http://www.hinezumilabs.org</a>
 - <a href="http://poetry.freaknet.org">http://poetry.freaknet.org</a>
</pre>
EOF;

print $contacts;

	
?>
