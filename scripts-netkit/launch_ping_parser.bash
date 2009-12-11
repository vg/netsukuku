#!/bin/bash

tcpdump -i any -s 1500 -n 'icmp[icmptype] == icmp-echo or icmp[icmptype] == icmp-echoreply' &> /tmp/ping_parser.log &

