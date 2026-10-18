#!/bin/sh
# restore stock boot, wipe pmOS userdata head so Android reformats it
cd ~/p80h-work/out
M="sudo $HOME/src/mtk-venv/bin/python $HOME/src/mtkclient/mtk.py"
for i in $(seq 1 60); do
  $M w boot,userdata $HOME/p80h-backup/parts2/boot.bin,zero64.bin > ../wrest.log 2>&1
  [ "$(grep -c '^.*Wrote ' ../wrest.log)" -ge 2 ] && { echo DONE > ../wrest.status; exit 0; }
  sleep 2
done
echo FAILED > ../wrest.status
