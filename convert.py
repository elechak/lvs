#! /usr/bin/python


#convert netlist from electric format to homogeneous format for nethash
import sys

f = file(sys.argv[1], "r")

labels =  "-labels" in sys.argv

keys = {}
sets=[]
nodes=[] # holds the transitor nodes
names={}



lines = f.readlines()

for line in lines:
    data = line.split()

    if line.startswith("create instance"):
        pass


        if data[2][1:4] == "mos":
            nodes.append(data[2] + "g")
            nodes.append(data[2] )
        else:
            pass
            #nodes.append(data[2])

    elif line.startswith("export"):
        names[data[1]+data[2]] = data[3]

    elif line.startswith("set node-name"):
        if data[2][1:4] == "mos":
            names[data[2]+data[3]] = data[4]
        else:
            names[data[2]] = data[4]



    elif line.startswith("connect"):

        a=data[1]
        b=data[3]

        if data[1][1:4] == "mos":
            a = data[1] +data[2]

        if data[3][1:4] == "mos":
            b = data[3] +data[4]


        if keys.has_key(a) and keys.has_key(b):
            s = keys[a]
            s.update(keys[b])
            for k in keys[b]:
                keys[k] = s
            sets.remove(keys[b])

        elif keys.has_key(a):
            s = keys[a]
            s.add(b)
            keys[b] = s

        elif keys.has_key(b):
            s = keys[b]
            s.add(a)
            keys[a] = s

        else:
            s = set([a,b])
            sets.append( s )
            keys[a] = s
            keys[b] = s



c =0

for k in nodes:
    if k.endswith("g"):
        print c, nodes.index(k[:-1])
    elif labels:
        if k.startswith("n"):
            print "L",c,"'NTRANS'"
        elif k.startswith("p"):
            print "L",c,"'PTRANS'"
    c +=1

for s in sets:

    #find label for sets
    if labels:
        for k,v in names.items():
            if k in s:
                print "L",c,"'%s'"%v
                break

    # print the node that everyone else connects to
    print c,

    # now print the connecting nodes
    for d in s:

        if d in nodes:
            print nodes.index(d),

        elif d[:-1] in nodes:
            print nodes.index(d[:-1]),

    print

    c+=1








