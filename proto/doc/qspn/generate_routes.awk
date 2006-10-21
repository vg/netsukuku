#!/bin/awk -f
#
# generate_routes.awk
# --
# 
# Given a graph on standard input it prints all the routes of each node.
#
# The format of input is very restricted: each node is rapresented by a
# alphanumeric character (case sensitive). On each line there has to be a
# description of a link. If the node A is linked to the node B, you've also to
# specify that B is linked to A.
# Example:
#
# printf "AB\nBA\nAC\nCA\nBD\nDB\nCD\nDC" | generate_routes.awk
#
#  The above string rapresents this graph:
#
#	  B
#  	 / \
#  	A   D
# 	 \ /
# 	  C
# The output of generate_routes.awk is:
#	ABDC
#	ACDB
#	BACD
#	BDCA
#	CABD
#	CDBA
#	DBAC
#	DCAB
#
# AlpT (@freaknet.org)
#

function xjoin(array, sep,   result, i)
{
	if(!sep)
		sep=" "
	for (i in array)
		result = (result (result?sep:"") array[i])
	return result
}

function xjoini(array, sep,   result, i)
{
	if(!sep)
		sep=" "
	for (i=1; i in array; i++)
		result = (result (result?sep:"") array[i])
	return result
}

function _gen_routes(links, prefix, node,       i,x,o,str,p, deepened)
{
	if(!((node,1) in links)) {
		routes[++gblidx]=prefix
		print "end " prefix > rp_file
		return
	}

	deepened=0
	for(o=1; (node,o) in links; o++) {
		print "node " node, "prefix", prefix,\
		      "node["o"]", links[node,o] > rp_file
		if(prefix ~ links[node,o]) {
			print "pre found, continue;" > rp_file
			continue
		}

		p=prefix links[node,o]
		_gen_routes(links, p, links[node,o])
		deepened=1
	}
	if(!deepened) {
		routes[++gblidx]=prefix
		print "exaust " prefix > rp_file
	}
}

function gen_routes(links)
{
	for(i in idxarr)
		_gen_routes(links, idxarr[i], idxarr[i])
}

{
	idx=++links[$1,"idx"]
	links[$1,idx]=$2
	if(idx == 1)
		idxarr[++count]=$1
}

BEGIN {
	FS=""
	count=0
	gblidx=0
	rp_file="./gr.log"
	print "date" |& "/bin/sh"
	"/bin/sh" |& getline date
	print "----- ", date," -----\n" > rp_file
	close("/bin/sh")
}

END {
	asort(idxarr)
	print "links " xjoin(links) > rp_file
	print "idxarr " xjoini(idxarr) > rp_file
	print "" > rp_file

	gen_routes(links)
	asort(routes)
	print xjoini(routes, "\n")

	close(rp_file)
}
