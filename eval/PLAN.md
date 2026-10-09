# Evaluation & Development Plan for wsfb-player

This document provides context, performance evaluation goals, and roadmap for developers and AI assistants.

## 1. Project Background & Goals
`wsfb-player` is a lightweight video player designed for NetBSD's native `wsdisplay` framebuffer (`/dev/ttyE0`, `/dev/fb0`).
It runs without X11, Wayland, or legacy OpenMAX APIs.

## 2. Benchmark & Evaluation History
- **armv6 (Raspberry Pi 1 / Zero)**:
  - Software decoding for 720p H.264 achieves ~17.5 FPS (CPU-bound).
  - Full-screen YUV420P -> RGB32 conversion (`swscale`) lowers throughput to ~7.0-11.5 FPS.
  - Adaptive frame skipping handles delayed frames gracefully.
- **armv7 (Raspberry Pi 2 / 3)**:
  - Multi-threaded decoding (`thread_count = 4`) and NEON flags improve throughput (~9.4 FPS for 720p, higher for 480p).

## 3. Evaluation Tools in `eval/`
- `eval_cpu_decode.c`: Benchmark pure FFmpeg decoding throughput.
- `eval_wsfb_sync.c`: Test native `wsdisplay` rendering with PTS / interval adaptive timer sync.

## 4. Roadmap & Future Scope

### Short-Term Goals (Application Layer)
- [ ] **Audio Support**: Implement `/dev/audio` (or `/dev/sound0`) PCM output using `libswresample` with Audio-Master-Clock A/V synchronization.
- [ ] **Endianness Verification**: Verify Big-Endian color output (`AV_PIX_FMT_0RGB`) on PowerPC architectures (e.g., Nintendo Wii U).
- [ ] **Dirty Region Optimization**: Optimize UI / OSD rendering to minimize CPU memory-bandwidth overhead.
- [ ] **pkgsrc Integration**: Package `wsfb-player` as `multimedia/wsfb-player`.

### Long-Term Goals (Kernel / Driver Layer)
- [ ] **NetBSD Broadcom VC4 DRMKMS Port**:
  - Port Linux `drivers/gpu/drm/vc4` DRM/KMS driver stack into NetBSD's `sys/external/bsd/drm2`.
  - Implement FDT attachment glue (`vc4_fdt.c`) for Broadcom BCM2835/2836/2837/2711 (`/soc/gpu`, `/soc/v3d`, `/soc/hdmi`).
  - Enable `/dev/dri/card0` support on NetBSD/evbarm to unlock hardware-accelerated DRM rendering (`mpv --vo=drm`).

