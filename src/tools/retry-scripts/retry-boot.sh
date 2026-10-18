#!/bin/sh
cd ~/p80h-work/out
for i in $(seq 1 60); do
  sudo ~/src/mtk-venv/bin/python ~/src/mtkclient/mtk.py w boot boot-probe.img > ../wboot.log 2>&1
  if grep -q "Wrote boot-probe.img" ../wboot.log; then echo DONE > ../wboot.status; exit 0; fi
  sleep 2
done
echo FAILED > ../wboot.status
