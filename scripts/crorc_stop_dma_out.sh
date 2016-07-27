#!/bin/bash

DEV=0
SCRIPTPATH=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
LOGPATH=$SCRIPTPATH/log
BINPATH=$LIBRORC_BUILD/tools_src

for CH in {0..11}
do
  PID=${LOGPATH}/pgdma_$(hostname)_${DEV}_${CH}.pid
  if [ -f $PID ]; then
    kill -s 2 `cat $PID`
  else
    echo "No PID file found for device ${DEV} channel ${CH}"
  fi
done
