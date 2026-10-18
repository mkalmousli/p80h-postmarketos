# p80hd: the on-device daemon

`p80hd` is a small static arm64 daemon (`src/daemon/`, plain C, no dependencies). The image builder puts it into the
boot image's initramfs and starts it as soon as the USB network is up, **before** the root filesystem is looked at.
So it is reachable whatever state the system on `userdata` is in, and it survives `switch_root`.

It listens on `172.16.42.1:4444` (the USB-net address only; it is an unauthenticated root service, a
bring-up tool, not something to ship on a finished device).

## Robustness
- A supervisor process owns the listening socket and restarts the worker if it ever crashes.
- One forked child per connection, at most 16 at once, 10 s to send the request line, keep-alive on every socket.
- Every request runs in PID 1's root (`/proc/1/root`), so after `switch_root` it sees the real system.
- `PUT` writes `path.p80h-tmp` and renames; it never creates files under `/dev`.

## Protocol (request line, then payload)
| request | reply |
|---|---|
| `PING` | `p80hd <version>` |
| `STATUS` | one JSON line (stage `initramfs`/`rootfs`, uptime, load, battery, memory) |
| `EXEC [secs] <cmd>` | output, then `@@EXIT n` or `@@TIMEOUT` (process group is killed on timeout) |
| `SHELL <rows> <cols>` | `OK`, then a raw pty session |
| `PUT <path> <size> [mode]` + bytes | `OK` / `ERR ...` |
| `GET <path> [offset [len]]` | `<size>` line + bytes |
| `REBOOT <mode>` | `OK`; mode is a reason string for LK (`recovery`, `bootloader`), empty, or `off` |
| `INJECT <node> <size>` + bytes | raw `struct input_event`s written to an input device |

Tests: `uv run python -m unittest discover -s tests` builds the daemon natively and drives it over loopback.
