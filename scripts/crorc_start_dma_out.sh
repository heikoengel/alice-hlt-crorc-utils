#!/bin/bash
#
# Start 12 PatterGenerator DMA Channels in background

DEV=0
SIZE=64
SCRIPTPATH=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
LOGPATH=$SCRIPTPATH/log
BINPATH=$(which crorc_dma_out)

mkdir -p $LOGPATH

for CH in {0..3}
do
  PID=${LOGPATH}/pgdma_$(hostname)_${DEV}_${CH}.pid
  LOG=${LOGPATH}/pgdma_$(hostname)_${DEV}_${CH}
  echo "Starting PatterGenerator DMA on device ${DEV} Channel ${CH}"
  daemonize -o $LOG.log -e $LOG.err -p $PID -l $PID ${BINPATH} --dev $DEV --ch $CH --packetsize 128
done
