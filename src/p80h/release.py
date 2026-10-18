"""Stage built images for a public GitHub release: copy to local/release/<tag>, write SHA256SUMS, optionally upload."""
import hashlib
import shutil
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FORBIDDEN = ("dumps", "stock-extracted", "nvram", "proinfo", "persist", "protect", "seccfg", "secro", "hwparam")


def stage(tag: str, files: list[Path], publish: bool = False) -> str:
    dest = REPO / "local" / "release" / tag
    dest.mkdir(parents=True, exist_ok=True)
    for f in files:
        f = f.resolve()
        if any(w in str(f).lower() for w in FORBIDDEN):
            raise SystemExit(f"refusing {f}: device-private or stock-derived data is never published")
        shutil.copy2(f, dest / f.name)
    sums = [f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}" for p in sorted(dest.iterdir()) if p.name != "SHA256SUMS"]
    (dest / "SHA256SUMS").write_text("\n".join(sums) + "\n")
    if publish:
        subprocess.run(["gh", "release", "create", tag, *map(str, dest.iterdir()), "--title", tag,
                        "--notes-file", str(REPO / "docs" / "releasing.md")], check=True, cwd=REPO)
    return f"staged {len(files)} files in {dest}" + (" and published" if publish else " (not published; add --publish)")
