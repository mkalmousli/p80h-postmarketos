#!/bin/sh
# write pmOS boot + rootfs through BROM, retrying until both writes succeed
cd ~/p80h_d8c1_g/out
M="sudo $HOME/src/mtk-venv/bin/python $HOME/src/mtkclient/mtk.py"
for i in $(seq 1 80); do
  $M w recovery,userdata rescue.img,root.img > ../../p80h-work/wsetup.log 2>&1
  [ "$(grep -c 'Wrote ' ../../p80h-work/wsetup.log)" -ge 2 ] && { echo DONE > ../../p80h-work/wsetup.status; exit 0; }
  sleep 2
done
echo FAILED > ../../p80h-work/wsetup.status
