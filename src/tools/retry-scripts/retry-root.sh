#!/bin/sh
cd ~/p80h-work/out
for i in $(seq 1 60); do
  sudo ~/src/mtk-venv/bin/python ~/src/mtkclient/mtk.py w userdata root.img > ../wroot.log 2>&1
  if grep -q "Wrote root.img" ../wroot.log; then echo DONE > ../wroot.status; exit 0; fi
  sleep 2
done
echo FAILED > ../wroot.status
