#!/bin/sh
# write pmOS boot + rootfs through BROM, retrying until both writes succeed
cd ~/p80h_d8c1_g/out
M="sudo $HOME/src/mtk-venv/bin/python $HOME/src/mtkclient/mtk.py"
for i in $(seq 1 80); do
  $M w userdata root.img > ../../p80h-work/wroot2.log 2>&1
  [ "$(grep -c 'Wrote ' ../../p80h-work/wroot2.log)" -ge 1 ] && { echo DONE > ../../p80h-work/wroot2.status; exit 0; }
  sleep 2
done
echo FAILED > ../../p80h-work/wroot2.status
