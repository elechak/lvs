#! /usr/bin/python

import os
import sys



os.system("rm -f stage*")

print "Layout ../lef/%s.lef" % sys.argv[1]
print "   stage1: Extracting"
os.system("extract ../lef/%s.lef >stage1" % sys.argv[1])

print "   stage2: Converting stage1 to homogeneous graph"
f = file("stage1" , "r")
outfile = file("stage2", "w")

max_id = 0
gate_id= 0


if "-labels" in sys.argv:

    # LABELS
    while 1:
        line = f.readline()
        if not line: break

        data = line.split()

        for d in data:
            id,name = d.split(":")

            id = id.strip()
            name = name.strip()

            if id[-1] != "g":
                print >> outfile, "L %s %s" % (id, name)

    f.seek(0);




# handle non gate connections
while 1:
    line = f.readline()
    if not line: break

    data = line.split()
    out = []

    for d in data:
        id,name = d.split(":")

        id = id.strip()
        name = name.strip()

        if id[-1] != "g":
            if int(id) > max_id:
                max_id = int(id)
            out.append(id)

    if len(out)>1:
        for c in out:
            print >> outfile, c,
        print >> outfile


#print "MAX:" , max_id

gate_id = max_id +1
gates={}

# handle gate connections
f.seek(0);
while 1:
    line = f.readline()
    if not line: break

    data = line.split()
    out = [data[0].split(":")[0].strip()]

    for d in data:
        id,name = d.split(":")

        id = id.strip()
        name = name.strip()

        if id[-1] == "g":
            i=id[0:-1]

            try:
                id = gates[i]
            except:
                gates[i] = gate_id
                id =gate_id
                gate_id +=1

            out.append(id)

    if len(out)>1:
        for c in out:
            print >> outfile,c,
        print >> outfile


f.close()

# connect gate to source and drain
for k,v in gates.items():
    print >> outfile, k,v
print >> outfile

outfile.close()



print "   stage2.hash: Hashing stage2"
os.system("nethash stage2 -size %i -depth 32" % gate_id)

print "   stage3: Prepare stage2.hash for comparison"
os.system("netview stage2.hash | sort -n >stage3")

print

# Schematic
print "Schematic ../sch/%s.txt" % sys.argv[1]
print "   stage4: convert netlist to homogeneous graph"
if "-labels" in sys.argv:
    os.system("./convert.py ../sch/%s.txt -labels >stage4" % sys.argv[1])
else:
    os.system("./convert.py ../sch/%s.txt >stage4" % sys.argv[1])

print "   stage4.hash: Hashing stage4"
os.system("nethash stage4 -size %i -depth 32" % gate_id)

print "   stage5: Prepare stage4.hash for comparison"
os.system("netview stage4.hash | sort -n >stage5")

print
print


# compare stage3 and stage5


stage3 = file("stage3", "r")
stage5 = file("stage5", "r")

iso=1

while 1:
    line3 = stage3.readline()
    if not line3: break

    line5 = stage5.readline()
    if not line5: break

    hash3,id3 = line3.split()
    hash5,id5 = line5.split()

    if hash3 == hash5:
        pass
        #print id3+"\t"+id5
    else:
        iso=0
        break;

if iso: print "Matched"
else: print "Failed - see stage3 and stage5"
















