#!/bin/sh
cd ~/p80h-work/out
for i in $(seq 1 40); do
  sudo ~/src/mtk-venv/bin/python ~/src/mtkclient/mtk.py w userdata zero64.bin > ../wzero.log 2>&1
  grep -q "Wrote zero64.bin" ../wzero.log && { echo DONE > ../wzero.status; exit 0; }
  sleep 2
done
echo FAILED > ../wzero.status
