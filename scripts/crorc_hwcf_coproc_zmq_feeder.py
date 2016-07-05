#!/usr/bin/python

import argparse
import sys
import os
import re
import subprocess
import time
import zmq

def ddlid2patch( DDL_ID ):
  if DDL_ID >= 768 and DDL_ID < 840:
    patch = (DDL_ID % 2)
  elif DDL_ID >= 840 and DDL_ID < 984:
    patch = (DDL_ID % 4) + 2
  else:
    raise ValueError, "DDL ID out of range: "+str(DDL_ID)
  return patch

parser = argparse.ArgumentParser()
parser.add_argument('-i', '--indir', help='directory containing raw DDL source files', required=True, type=str)
parser.add_argument('-r', '--refdir', help='directory containing emulated HWCLUST1 reference files', type=str)
parser.add_argument('-o', '--outdir', help='output directory', type=str)
parser.add_argument('-p', '--patch', help='TPC patch', type=int, default=-1)

#parser.add_argument('-m', '--mapdir', help='directory containing emulated HWCLUST1 reference files', required=True, type=str)
args = parser.parse_args()

if not os.path.isdir(args.indir):
  sys.stderr.write("ERROR: indir %s could not be found\n" % (args.indir))
  sys.exit(-1)
if args.refdir and not os.path.isdir(args.refdir):
  sys.stderr.write("ERROR: refdir %s could not be found\n" % (args.refdir))
  sys.exit(-1)

#ddlfiles = []
#mapfilepath = []
#for patch in range(0,6):
#  ddlfiles.append( [] )
#  mapfilepath.append(os.path.join(args.mapdir, "FCF_Mapping_Patch%d.data" %(patch)))
#  if not os.path.isfile(mapfilepath[patch]):
#    sys.stderr.write("ERROR: Mapfile %s not found.\n" % (mapfilepath))
#    sys.exit(-1)

ctx = zmq.Context()
skt = [None]*6
if args.patch < 0:
  for i in range(6):
    skt[i] = ctx.socket(zmq.PUSH)
    skt[i].connect("tcp://localhost:%d" % (5555 + i))
else:
  ch = args.patch
  skt[ch] = ctx.socket(zmq.PUSH)
  skt[ch].connect("tcp://localhost:%d" % (5555 + ch))


pushcount = 0

for root, dirnames, filenames in os.walk(args.indir):
  for filename in filenames:
    idgrp = re.search("TPC_(\d+).ddl", filename)
    if not idgrp:
      continue
    ddlid = int(idgrp.group(1))
    patchid = ddlid2patch(ddlid)
    rel_inpath = os.path.relpath(os.path.join(root, filename), args.indir)
    infilename = os.path.abspath(os.path.join(root, filename))
    #refpath_tmp = os.path.join(args.refdir, rel_inpath)
    reffilename = "" #refpath_tmp.replace("TPC_", "TPC_HWCLUST1_")[:-4]
    if args.outdir:
      outpath_tmp = os.path.join(args.outdir, rel_inpath)
      outfilename = os.path.abspath(outpath_tmp.replace("TPC_", "FCF_"))
      if not os.path.exists(os.path.dirname(outfilename)):
        os.makedirs(os.path.dirname(outfilename))
    else:
      outfilename = ""
    #if not os.path.isfile(reffilename):
    #  sys.stderr.write("ERROR: Reffile %s not found.\n" % (reffilename))
    #  continue
    #entry = { 'ddlid':ddlid, 'infile':infilename, 'reffile':reffilename, 'outfile':outfilename }
    #ddlfiles[patchid].append(entry)
    if (skt[patchid]):
      skt[patchid].send("%s;%s;%s" % (infilename, outfilename, reffilename))
      pushcount += 1

for i in range(6):
  if (skt[i]):
    skt[i].send(";;;");
print "Pushed %d files." % (pushcount)
exit(0)

#procs = []
#for patch in range(0, 6):
#  #cmd = "hwcf_coproc -n 0 -c %d -m %s" % (patch, mapfilepath[patch])
#  cmd = "crorc_hwcf_coproc -n 0 -c 0 -m %s" % (mapfilepath[patch])
#  filecount=0
#  for entry in ddlfiles[patch][:4000]:
#    cmd += " -i %s" % (entry['infile'])#, entry['reffile'])
#    if entry['outfile']:
#      cmd += " -o %s" % (entry['outfile'])
#    filecount += 1
#  #cmd = cmd.split(' ')
#  print "patch %d: %d file pairs, %d cmd args" % (patch, filecount, len(cmd))
#  #print cmd
#  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
#  out, err = p.communicate()
#  #p.wait()
#  if err:
#    print ' '.join(cmd)
#    print err
#  #procs.append(subprocess.Popen(cmd))
  
#for p in procs:
  #p.wait()
  
