#!/bin/awk -f
#
#
# AlpT (@freaknet.org)
#

function xlog(ra, rb, op, rf,           str1)
{
	if(!ra && rb)
		str1="\""rb"\" "
	else if(!rb && ra)
		str1="\""ra"\" "
	else if(!rb && !ra)
		str1=""
	else
		str1="\""ra"\" + " "\""rb"\" "

	print str1 "=" op "=> \""rf"\"" > rp_file
}

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



# Written by Steffen Schuler
function collapse(s,p,q,    t, pre, rest) {
	t=substr(s,p,q-p)
	pre=substr(s,1,p-1)
	rest=substr(s,q)
	while (substr(rest,1,q-p)==t) {
		rest=substr(rest,q-p+1)
	}
	return pre t rest
}


# Written by Steffen Schuler
function remove_dup(route,     i,j,k,bc)
{
	return 0

	# 
	# (.+)+  ==>  X&Y
	# XABCABCABCY  ==> XABCY
	#
	for(i=1;i<length(route);++i) {
		b=route
		k=0
		for(j=int((length(route)+i+1)/2);\
		    !collapsed && j>i;--j) {
			c=collapse(route,i,j)
			if(length(c) < length(b)) {
				b=c
				k=j
			}
		}
		route=b
		if (k > 0) {
			i=k-1
		}
	}
	return route
}

function extract_all_extreme_cycles(route,arr,     rl,idx,ma,i,c,cycle_len,x)
{
	# A...AXYZ  ==> A...A
	#  or
	# XYZA...A  ==> A...A

	idx=0
	for(cycle_len=1; cycle_len <= length(route)-2; cycle_len++) {
		if(substr(route,1,1) ==\
		   substr(route,cycle_len+2,1)) {
			   arr[++idx,"cycle"]=substr(route,2,cycle_len)
			   arr[idx,"node"]=substr(route,1,1)
			   arr[idx,"start"]=""
			   arr[idx,"end"]=substr(route,cycle_len+3)
                                   # print "arr " arr[idx,"start"], arr[idx,"node"], \
                                   #       arr[idx,"cycle"], arr[idx,"node"],\
                                   #       arr[idx,"end"] > rp_file
		   }
	}

	rl=length(route)
	for(cycle_len=1; cycle_len <= length(route)-2; cycle_len++) {
		x=rl-cycle_len-1
		if(substr(route,rl,1) ==\
		   substr(route,x,1)) {
			   arr[++idx,"cycle"]=substr(route,x+1,cycle_len)
			   arr[idx,"node"]=substr(route,x,1)
			   arr[idx,"start"]=substr(route,1,x-1)
			   arr[idx,"end"]=""
                                   # print "arr " arr[idx,"start"], arr[idx,"node"], \
                                   #       arr[idx,"cycle"], arr[idx,"node"],\
                                   #       arr[idx,"end"] > rp_file
		   }
	}

	return idx ? 1 : 0
}


function extract_all_cycles(route,arr,     idx,ma,i,c,cycle_len,x)
{
	# 123A...AXYZ  ==> A...A

	idx=0
	for(cycle_len=1; cycle_len <= length(route)-2; cycle_len++) {
		for(x=1; x <= length(route)-(o+2)+1; x++) {
			if(substr(route,x,1) ==\
			   substr(route,x+cycle_len+1,1)) {
				   arr[++idx,"cycle"]=substr(route,x+1,cycle_len)
				   arr[idx,"node"]=substr(route,x,1)
				   arr[idx,"start"]=substr(route,1,x-1)
				   arr[idx,"end"]=substr(route,x+cycle_len+2)
                                   # print "arr " arr[idx,"start"], arr[idx,"node"], \
                                   #       arr[idx,"cycle"], arr[idx,"node"],\
                                   #       arr[idx,"end"] > rp_file
			   }
		   }
	   }

	return idx ? 1 : 0
}

# 
# Returns 1 if it valid
#
function is_route_valid(route,		rac, i)
{
	# 
	# Example of a NON valid route:
	#	...ABA...
	#

	for(i=1; i<=length(route)-2; i++)
		if(substr(route, i,1) == substr(route, i+2,1)) {
			#print "backward route not valid: " route
			return 0
		}

        # if(extract_all_cycles(route, rac)) {
        #         for(i=1; (i,"cycle") in rac; i++) {
        #                 #rac[i,"cycle"]=remove_dup(rac[i,"cycle"])
        #                 #print "rac["i"]", rac[i,"cycle"]
        #                 if(length(rac[i,"cycle"]) == 1) {
        #                         return 0
        #                 }
        #         }
        # }
	return 1
}

function simplify(routes,      i,o,e,x,ra,rat,rac,ra_length,rb,rstr,rstr_old,rgxa,rgxb)
{
	rstr_old=xjoin(routes)
	printf "%-10s %6s\n", "old", rstr_old > rp_file

	for(i in routes) {
		ra=routes[i]

		if((rat=remove_dup(ra)) && is_route_valid(rat)) {
			routes[i]=rat
			if(ra != routes[i])
				xlog(ra, "", "dup", routes[i])
			ra=routes[i]
		}


		#
		# 123ABC + ABCXYZ ==> 123ABCXYZ
		# 123ABCXYZ + ABC ==> 123ABCXYZ
		#	
		ra_length=length(ra)
		for(e=ra_length; e >= 1; e--) {
			rstr = substr(ra, 1, e)
			for(o in routes) {
				if(o == i)
					continue

				rb=routes[o]
				rgxb = ("(" rstr ")")
				if(e == ra_length)
					# 123ABCXYZ + ABC ==> 123ABCXYZ
					rgxb = rgxb
				else
					# 123ABC + ABCXYZ ==> 123ABCXYZ
					rgxb = rgxb "$"
				if(sub(rgxb, ra, rb) &&
				   is_route_valid(rb)) {

					   xlog(routes[i], routes[o], \
						"join", rb);
					   routes[o]=rb
					   delete routes[i]
					   break
				   }
			   }
			if(!(i in routes))
				break
		}
		if(!(i in routes))
			continue

		#
		# XA...A + XAY  ==>  XA...AY
		# A...AZ + YAZ  ==>  YA...AZ
		# A...A  + YAZ  ==>  YA...AZ
		#
		delete rac
		if(extract_all_extreme_cycles(ra, rac)) {
			for(x=1; (x,"cycle") in rac; x++) {
                                # print "cycle " x ": ","N" rac[x,"node"],
                                #         "S"rac[x,"start"],\
                                #         "E"rac[x,"end"],  "C"rac[x,"cycle"]
				for(o in routes) {
					if(o == i)
						continue

					rb=routes[o]

					# XA...A + XAY  ==>  XA...AY
					rgxa="^" rac[x,"start"] rac[x,"node"]
					if(!rac[x,"end"] &&
					   sub(rgxa, (rac[x,"start"] \
					   rac[x,"node"] rac[x,"cycle"] \
					   rac[x,"node"]), rb) && 
					   	is_route_valid(rb)) {

					   xlog(routes[i], routes[o], \
						"xa", rb);
					   routes[o]=rb
					   delete routes[i]
					   break
					}
		
					# A...AZ + YAZ  ==>  YA...AZ
					rgxb=rac[x,"node"] rac[x,"end"] "$"
					if(!rac[x,"start"] &&
					      sub(rgxb, (rac[x,"node"] rac[x,"cycle"] \
					   rac[x,"node"] rac[x,"end"]), rb) &&
					   	is_route_valid(rb)) {

					   xlog(routes[i], routes[o], \
						"az", rb);
					   routes[o]=rb
					   delete routes[i]
					   break
					}

					# A...A + YAZ  ==>  YA...AZ
					if(!rac[x,"start"] && !rac[x,"end"]) {
						rgxb=rac[x,"node"] 
						if(sub(rgxb, (rac[x,"node"] \
						      rac[x,"cycle"] \
						      rac[x,"node"]), rb) &&
					   		is_route_valid(rb)) {

							      xlog(routes[i], routes[o], \
								   "yaz", rb);
							      routes[o]=rb
							      delete routes[i]
							      break
						      }
				       }
				 }
				      if(!(i in routes))
					      break
			}
		}
	}

	rstr=xjoin(routes)
	printf "%-10s %6s\n\n", "new", rstr > rp_file
	if(rstr == rstr_old)
		return
	else
		simplify(routes)
}

{
	if(length <= 1) {
		if(length == 1)
			print "invalid route: ", $0 > "/dev/stderr"
		next
	}
	gbl_routes[++count] = $0
}

BEGIN {
	rp_file="./sr.log"
	print "date" |& "/bin/sh"
	"/bin/sh" |& getline date
	print "----- ", date," -----\n" > rp_file
	close("/bin/sh")
}

END {
	simplify(gbl_routes)

	asort(gbl_routes)
	for(xi in gbl_routes)
		print gbl_routes[xi]
	close(rp_file)
}
