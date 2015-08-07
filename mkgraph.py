#! /usr/bin/python

import random
import sys

r = random.Random()

try:
    num_nodes = int(sys.argv[1])
    num_connections = int(sys.argv[2])
    net = set([(r.randrange(num_nodes),r.randrange(num_nodes))  for x in range(num_connections) ])

    for c in net:
        if c[0] == c[1]: continue
        print c[0], c[1]
except:
    print "Usage: "
    print "mkgraph.py  number_of_nodes  number_of_connections"


