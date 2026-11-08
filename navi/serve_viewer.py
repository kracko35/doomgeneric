"""Expose the running guest via loopback-only noVNC (optional)."""
import json, socket, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
with socket.socket() as probe:
    if probe.connect_ex(('127.0.0.1',6097))==0:
        raise SystemExit('Port 6097 is already occupied; no existing service was changed')
with (HERE/'evidence/viewer.log').open('ab') as log:
    p=subprocess.Popen(['websockify','--web=/usr/share/novnc','127.0.0.1:6097','127.0.0.1:5907'],
        stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
print(json.dumps({'viewer_pid':p.pid,'listen':'127.0.0.1:6097','vnc':'127.0.0.1:5907'}))
