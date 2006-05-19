<?php

$faq = <<<EOF
<pre id="filez">

  0. General
        Q: What is Netsukuku?
        Q: What are its features?
        Q: Why did you choose that name?
        Q: What does it mean &quot;it uses chaos and fractals&quot;?
        Q: Why another p2p network?
        Q: Ehi! You're crazy. This shit will not work!
        Q: Where are current Netsukuku networks that I can connect to?
        Q: What can I do to help the development of Netsukuku? How can I
           contribute to its growth?

  1. Technical
        Q: Does it scale in a network with A LOT of nodes?
        Q: What do you intend to do to solve the IP unicity problem?
        Q: Does it really works?
        Q: Netsukuku is separated from Internet. How?
        Q: How can I join in Netsukuku?
        Q: And how does a new node begin to locate any of the other nodes in
           the network?
        Q: Will you provide &quot;Internet to Netsukuku&quot; tunnels?
        Q: Aside from what I hack myself I was wondering what can be done on
           the Netsukuku network?
        Q: Will we be able to host websites anytime soon?
        Q: Will glibc be able to resolve names for the ANDNA system?
        Q: What sort of performance does Netsukuku have? Is it any good for
           voice chat, video chat, games?

  2. Software
        Q: On what OS does it run?
        Q: Will Netsukuku be ported to Windows?
        Q: Will Netsukuku be ported to PSP / Nintendo DS / wifi phones / PDAs?
        Q: How does it join the network?
        Q: For using a wifi link do I need of an access point? What to do?
        Q: Why the code is not written in java?

--


/                \
   0. General
\                /

Q: What is Netsukuku?
A: Netsukuku is a mesh network or a p2p net system that generates and sustains
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

   For more information read the section &quot;2.3  So, WTF is it?&quot; of the
   document ( <a href="http://netsukuku.freaknet.org/doc/netsukuku">http://netsukuku.freaknet.org/doc/netsukuku</a> )

Q: What are its features?
A: The complete list of features is here:
   <a href="http://netsukuku.freaknet.org/files/doc/misc/Ntk_features_list">http://netsukuku.freaknet.org/files/doc/misc/Ntk_features_list</a>

Q: Why did you choose that name?
A: Networked Electronic Technician Skilled in Ultimate Killing, Utility and
   Kamikaze Uplinking.
   But there is also another story: we were learning Japanese katakana with
   `slimeforest', a nice game for GNU/Linux.
   Unfortunately when we encountered the &quot;Network&quot; word, written in Japanese,
   we didn't know all the relative symbols, so the only katakana we were able
   to read were few and mixed with others the name was: Ne tsu ku ku.
   By the way, you can always think of any other deceitful and hidden
   meanings.

Q: What does it mean &quot;it uses chaos and fractals&quot;?
A: The Netsukuku protocol (Npv7) structures the entire net as a fractal and,
   in order to calculate all the needed routes which are necessary to connect a
   node to all the other nodes, it makes use of a particular algorithm called
   Quantum Shortest Path Netsukuku (QSPN).
   Here a fractal is meant as a highly clusterized graph of nodes.
   (For the in depth description of the map system in Netsukuku read the
   &quot;5.3  The truly Gnode^n for n&lt;=INFINITE&quot;  section in the document.)

   On the other hand, the QSPN is a meta-algorithm in the sense that it
   has to run on a real (or simulated) network. The nodes have to send the
   QSPN pkt in order to &quot;execute&quot; it. For this reason it is not always true
   that a determinate pkt will be sent before another one.
   This system allows to get the best routes without any heavy computation.
   (read the &quot;5.1   QSPN: Quantum Shortest Path Netsukuku&quot; section in the
   document).

Q: Why another p2p network?
A: Netsukuku is not a p2p net built upon the Internet. It is a physical
   network and it is a dynamic routing system designed to handle 2^128 nodes
   without any servers or central systems, in this way, it is possible to
   build a physical network separated from the Internet. Btw, read &quot;What is
   Netsukuku&quot;.

Q: Ehi! You're crazy. It won't work!
A: Ehi pal, this doesn't pretend to be _now_ the final solution to the meaning
   of life, the universe and everything. Why don't you contribute and give us
   useful hints from your great knowledge? If you want to help in the
   development, read the code and contact us ;)

Q: Where are current Netsukuku networks that I can connect to?
A: Simply we don't know and we can't, but the website team si developing a
   community portal which will ease the difficulty of coordination. (Think of
   Google maps).

Q: What can I do to help the development of Netsukuku? How can I contribute to
   its growth?
A: <a href="http://lab.dyne.org/Ntk_Grow_Netsukuku">http://lab.dyne.org/Ntk_Grow_Netsukuku</a>


/                \
   1. Technical
\                /

Q: Does it scale in a network with A LOT of nodes?
A: Simple and not accurate reasons for the scalability of Netsukuku (until there
   is the technical documentation with math background that is being written):
   1) the size of the maps is fixed: about 4Kb for the int_map and 16Kb for
      the ext_map.
   2) Not all the nodes sends a broadcast discovery.
   3) There are few floods for each discovery.
   4) When a node receives a flood it already has the routes without
      calculating anything.
   5) A flood is synchronized: the same flood starts at the same time for all
      the nodes.

   A first draft of the explanation of the Netsukuku scalability is available
   here: <a href="http://lab.dyne.org/Netsukuku_scalability">http://lab.dyne.org/Netsukuku_scalability</a>

Q: What do you intend to do to solve the IP unicity problem?
A: It is already solved: <a href="http://lab.dyne.org/Ntk_gnodes_contiguity">http://lab.dyne.org/Ntk_gnodes_contiguity</a>

Q: Does it really works?
A: ^_^

Q: Netsukuku is separated from Internet. How?
   Someone is building all new infrastructure? Who's paying for that?
A: Not at all, there is no need to pay. The best way to physical link two
   nodes is using the wifi. Nowadays, there are a lot of cool wifi
   technologies, which allows to link two nodes distant kilometres each other.
   In the city there would be no problems, it suffices only a node for
   each neighbourhood and the city will be completely covered.

Q: How can I join in Netsukuku?
A: Take out your wifi antenna, and start the Netsukuku daemon on the relative
   network interface, then wait and tell to do the same thing to all your
   friends ^_-

Q: And how does a new node begin to locate any of the other nodes in the
   network?
A: The Netsukuku radar sends echo packets about every 10 seconds, if someone
   replies it communicates with it.

Q: Will you provide &quot;Internet to Netsukuku&quot; tunnels?
A: Yes, they will be used to link close cities. Please read this for more
   information:
   <a href="http://lab.dyne.org/Ntk_Internet_tunnels">http://lab.dyne.org/Ntk_Internet_tunnels</a>

Q: Aside from what I hack myself I was wondering what can be done on the
   Netsukuku network?
A: Whatever you already do in the actual Internet. What the Netsukuku daemon
   does is to only set the routes in the kernel routing table.

Q: Will we be able to host websites anytime soon?
A: You can do it by now!

Q: Will glibc be able to resolve names for the ANDNA system?
A: ANDNA comes with a DNS wrapper so it is trasparent to all the programs
   which uses the glibc. Read &quot;man andna&quot;:
   <a href="http://netsukuku.freaknet.org/doc/manuals/html/andna.html">http://netsukuku.freaknet.org/doc/manuals/html/andna.html</a>

Q: What sort of performance does Netsukuku have? Is it any good for voice chat
   video chat?
A: What do you mean by `performance'?

   Network performance: it is dependent on the links quality. If the nodes are
   linked by 100Mbps cable you will feel like in a large LAN.
   The distance from yourself and the destination node is also relevant.
   Remember that the Netsukuku daemon chooses only the best way to reach
   the other nodes, but cannot improve the roads themself.

   Software performance: you really shouldn't worry about this:
   PID   USER  PRI  NI  SIZE  RSS  SHARE %CPU %MEM TIME CPU COMMAND
   18521 root  15   0   17708 1552 1164  0.0  0.3  0:00 0   ntkd


/                \
   2. Software
\                /

Q: On what OS does it run?
A: For now it runs only on GNU/Linux, but it is easy to port it on other OS.
   If you want to join in the development let us now ;)

Q: Will Netsukuku be ported to Windows?
A: Short answer: if you code the port, yes.
   Answer: We need coders for that. There are a lot of things to be done and
   the Windows port is what we care less.

Q: Will Netsukuku be ported to PSP / Nintendo DS / wifi phones / linux PDAs
   etc...
A: We are currently working on flashing Netsukuku on Access Points (like
   Linksys).

Q: For using a wifi link do I need of an access point? What to do?
A: You just need a wifi network card. Put it in ad-hoc mode using &quot;netsukuku&quot;
   as essid. ( man netsukuku_wifi:
   <a href="http://netsukuku.freaknet.org/doc/man_html/netsukuku_wifi.html">http://netsukuku.freaknet.org/doc/man_html/netsukuku_wifi.html</a> )

Q: Why the code is not written in java?
A: Are you kidding?


--

Q: My question is not answered here!
A: Contact us: <a href="http://netsukuku.freaknet.org">http://netsukuku.freaknet.org</a>
</pre>
EOF;

print $faq;
?>
