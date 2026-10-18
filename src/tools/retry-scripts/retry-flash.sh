#!/bin/sh
cd ~/p80h-work/out
for i in $(seq 1 60); do
  sudo ~/src/mtk-venv/bin/python ~/src/mtkclient/mtk.py w boot,userdata boot.img,root.img > ../wflash.log 2>&1
  if grep -q "Wrote root.img" ../wflash.log && grep -q "Wrote boot.img" ../wflash.log; then echo DONE > ../wflash.status; exit 0; fi
  sleep 2
done
echo FAILED > ../wflash.status
