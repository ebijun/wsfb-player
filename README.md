# wsfb-player

A lightweight, zero-dependency video player for NetBSD's `wsdisplay` / `wsfb` framebuffer.

`wsfb-player` uses **FFmpeg** for decoding and renders frames directly into NetBSD's native `/dev/ttyE0` or `/dev/fb0` framebuffer memory map (`WSDISPLAYIO_MODE_DUMBFB`), bypassing X11, Wayland, and legacy OpenMAX APIs.

## Features

- **Direct Framebuffer Rendering**: Renders directly to NetBSD `wsdisplay` (`/dev/ttyE0` / `/dev/fb0`).
- **FFmpeg 7 Support**: Fast H.264 video decoding with multi-threading support.
- **Adaptive Frame Synchronization**: Dynamic frame-skipping mechanism to prevent audio/time desynchronization on lower-spec hardware (e.g., Raspberry Pi 1/2/3, Wii U).
- **Safe Signal Handling**: Automatically restores the console text mode (`WSDISPLAYIO_MODE_EMUL`) upon `Ctrl+C` or `SIGTERM`.

## Prerequisites

On NetBSD, install FFmpeg 7 via `pkgin`:

```bash
pkgin install ffmpeg7

````

## Building

``` bash
make

```

## Usage

Run with root privileges (required for framebuffer device access):

```bash
./wsfb-player /path/to/video.mp4 [pixel_format]

```

### Pixel Format Option (for amd64 / x86 framebuffers)

If screen distortion or incorrect colors occur (e.g., on `amd64` GPUs), specify the pixel format explicitly:

``` bash
./wsfb-player /path/to/video.mp4 rgb0
# Supported options: bgr0 (default), rgb0, 0rgb

```

## License

2-Clause BSD License


