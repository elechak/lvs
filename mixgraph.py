#! /usr/bin/python

import random
import sys

r = random.Random()

f = open(sys.argv[1] , "r")

max = 0

data = []


while 1:
    line = f.readline()
    if not line: break
    x,y = line.split()
    x = int(x)
    y = int(y)

    if x > max :
        max = x

    if y > max:
        max = y

    data.append([x,y])


trans = range(0 , max+1)
r.shuffle(trans)


for d in data:
    d[0] = trans[d[0]]
    d[1] = trans[d[1]]

r.shuffle(data)

for d in data:
    print d[0] ,d[1]















