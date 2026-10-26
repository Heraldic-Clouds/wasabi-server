# libguac-client-mea (experimental)

A guacd protocol plugin that shows an Eleet desktop through the **MEA Stream
agent** instead of VNC/RDP. guacd takes the gateway's role on the agent link
(Type3-hypervisor-manager `stream/docs/PROTOCOL.md` §6):

1. TCP to the agent in the desktop container (default port 47100).
2. `HELLO` with a short-lived MEA stream token, H.264 only. The agent
   verifies it (signature, expiry, single use, desktop binding). A refused
   token ends the connection with `CLIENT_UNAUTHORIZED` (0x0301).
3. `VIDEO` access units are decoded with libavcodec and drawn to the default
   layer; Guacamole then re-encodes changed regions as usual.
4. Mouse and keyboard become MEA input events; a heartbeat runs every
   second, and the agent releases held input when the link drops.

Parameters: `hostname`, `port`, `token`, `profile` (economy/balanced/high,
clamped by the token), `fps`, `kbps`, `read-only`.

What it is for: comparing the agent's NVENC pipeline inside the existing
Guacamole path. It is not the gaming path: decoding and re-encoding in guacd
adds latency and CPU. Gaming Mode in the browser uses WebRTC. VNC stays the
standard desktop. Audio is not forwarded.

The MEA API does not hand out `mea` connections yet; to try it, mint a token
with `mea-stream-token` and create a connection with these parameters.

Build: enabled automatically when libavcodec, libavutil and libswscale are
found (`--disable-mea` to skip). End-to-end test with a real guacd, agent and
Xvfb:

```sh
BIN=<Type3 stream bin dir> GUACD_BUILD=$PWD python3 -I src/protocols/mea/e2e/test_mea_adapter.py
```

The older `game` plugin (in-process X11 capture + NVENC) is left unchanged;
see the MEA Stream analysis for its known issues. This plugin replaces its
purpose without touching it.
