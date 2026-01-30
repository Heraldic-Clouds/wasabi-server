# Guacamole Game Streaming Protocol

A custom Guacamole protocol for low-latency game streaming from Linux desktops using NVIDIA NVENC hardware encoding.

## Overview

The game protocol captures the X11 display, encodes it using NVENC (or software fallback), and streams it to Guacamole clients. It's optimized for:

- **Ultra-low latency** - Zero-delay NVENC encoding with no B-frames
- **High framerate** - Up to 240 FPS support
- **Hardware acceleration** - NVIDIA GPU encoding via FFmpeg NVENC
- **Full input support** - Keyboard and mouse injection via XTest

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   X11 Display   │────▶│  XShm Capture   │────▶│  NVENC Encode   │
│   (Game/App)    │     │  (60+ FPS)      │     │  (H.264/HEVC)   │
└─────────────────┘     └─────────────────┘     └────────┬────────┘
                                                         │
┌─────────────────┐     ┌─────────────────┐              │
│  Guacamole Web  │◀────│  guacd Daemon   │◀─────────────┘
│  or Android App │     │  (WebSocket)    │
└─────────────────┘     └─────────────────┘
```

## Dependencies

### Required
- X11 development libraries (`libx11-dev`)
- XShm extension (`libxext-dev`)
- XTest extension (`libxtst-dev`)
- FFmpeg with NVENC support:
  - `libavcodec-dev`
  - `libavutil-dev`
  - `libswscale-dev`

### For Hardware Encoding
- NVIDIA GPU (Maxwell or newer)
- NVIDIA drivers with NVENC support
- FFmpeg compiled with `--enable-nvenc`

## Building

```bash
cd guacamole-server

# Generate configure script
autoreconf -fi

# Configure with game protocol enabled
./configure --enable-game

# Build
make

# Install (optional)
sudo make install
```

### Configure Options

| Option | Description |
|--------|-------------|
| `--enable-game` | Enable game streaming protocol (default: yes if dependencies met) |
| `--disable-game` | Disable game streaming protocol |
| `--with-x11` | Specify X11 support (default: check) |

## Protocol Arguments

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `display` | string | `:0` | X11 display to capture |
| `framerate` | int | `60` | Target frames per second (1-240) |
| `bitrate` | int | `15000` | Target bitrate in kbps (500-100000) |
| `codec` | string | `h264` | Video codec: `h264` or `hevc` |
| `preset` | string | `p1` | NVENC preset: `p1` (fastest) to `p7` (best quality) |
| `cbr` | boolean | `true` | Use constant bitrate mode |
| `read-only` | boolean | `false` | Disable input injection |
| `width` | int | `0` | Width override (0 = auto) |
| `height` | int | `0` | Height override (0 = auto) |
| `force-lossless` | boolean | `false` | Force lossless compression |

## Connection Example

### guacamole.properties
```properties
game-hostname: localhost
game-display: :0
game-framerate: 60
game-bitrate: 20000
game-codec: h264
game-preset: p1
```

### JSON Connection
```json
{
  "name": "Game Stream",
  "protocol": "game",
  "parameters": {
    "display": ":0",
    "framerate": "60",
    "bitrate": "15000",
    "codec": "h264",
    "preset": "p1"
  }
}
```

## NVENC Presets

| Preset | Quality | Speed | Use Case |
|--------|---------|-------|----------|
| `p1` | Lowest | Fastest | Real-time gaming, lowest latency |
| `p2` | Low | Very Fast | Fast-paced games |
| `p3` | Medium-Low | Fast | General gaming |
| `p4` | Medium | Medium | Balanced |
| `p5` | Medium-High | Slow | Quality preference |
| `p6` | High | Slower | High quality streaming |
| `p7` | Highest | Slowest | Maximum quality |

For game streaming, `p1` or `p2` is recommended to minimize latency.

## Recommended Settings

### 1080p60 Gaming
```
framerate=60
bitrate=15000
codec=h264
preset=p1
```

### 1080p120 High Refresh
```
framerate=120
bitrate=25000
codec=h264
preset=p1
```

### 4K60 Streaming
```
framerate=60
bitrate=40000
codec=hevc
preset=p2
```

## Troubleshooting

### "Failed to initialize X11 capture"
- Ensure X11 is running on the specified display
- Check `DISPLAY` environment variable
- Verify XShm extension is available: `xdpyinfo | grep MIT-SHM`

### "Failed to initialize NVENC encoder"
- Check NVIDIA drivers are installed: `nvidia-smi`
- Verify FFmpeg has NVENC support: `ffmpeg -encoders | grep nvenc`
- Falls back to software encoding (libx264/libx265) if NVENC unavailable

### High Latency
- Use `preset=p1` for lowest latency
- Reduce `framerate` if CPU/GPU is overloaded
- Ensure `cbr=true` for consistent bitrate

### Input Not Working
- Verify XTest extension: `xdpyinfo | grep XTEST`
- Check `read-only` is not set to `true`
- Ensure guacd has permission to inject input

## File Structure

```
src/protocols/game/
├── Makefile.am          # Build configuration
├── client.c/h           # Client initialization and lifecycle
├── settings.c/h         # Argument parsing
├── user.c/h             # User join/leave handlers
├── game.c/h             # Main capture/encode loop
├── capture-x11.c/h      # X11/XShm screen capture
├── nvenc.c/h            # FFmpeg NVENC encoding
├── input.c/h            # XTest input injection
└── video-stream.c/h     # Guacamole video streaming
```

## License

Licensed under the Apache License, Version 2.0. See the LICENSE file for details.
