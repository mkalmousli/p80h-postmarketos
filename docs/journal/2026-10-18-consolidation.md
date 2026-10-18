# 2026-10-18 consolidation
Merged ~/p80h-backup, ~/p80h-work, ~/p80h_d8c1_g, ~/src/{mtkclient,pmaports,pmbootstrap}, ~/pmos-work into this folder.
Left outside: ~/src/linux (upstream clone, duplicate of trees/linux), ~/src/mtk-venv + tools-venv (recreatable venvs), ~/.cache/p80h (kernel cache), ~/src/rea-cli (unrelated).
Old bash scripts moved to tools/retry-scripts (hardcode old paths; do not run as is).
Restructured to 3 top folders: src/ (code), docs/, local/ (gitignored data). Paths in code updated (tree.py, image/agent.py, cli.py, trykernel.py).
