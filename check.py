#! /usr/bin/python

import commands
import os
import sys
import time

#x= int(sys.argv[1])
x = 50
#y = 100


for y in range(50000,100000):

    os.system("rm data*")

    os.system("mkgraph.py %i %i >data" % (x, y))
    os.system("mixgraph.py data >data2")

    c1 = time.time()
    os.system("nethash -size %i data" % x )
    os.system("nethash -size %i data2" % x )

    c2 = time.time()
    data1 = commands.getoutput("netview data.hash | sort -n").split("\n")
    data2 = commands.getoutput("netview data2.hash | sort -n").split("\n")


    for i in range(len(data1)):

        hash1, id1 = data1[i].split()
        hash2, id2 = data2[i].split()

        if hash1 != hash2:
            print "ERROR size=%i line=%i" % (x, i)

            for a in range(len(data1)):
                hash1, id1 = data1[a].split()
                hash2, id2 = data2[a].split()
                print a, hash1 , hash2 ,id1,id2
            sys.exit(1)

        #print hash1 , id1, "->", id2

    print x, y , c2-c1







