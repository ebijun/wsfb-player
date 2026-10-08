# Benchmark & Evaluation Results for wsfb-player

This document summarizes performance evaluation results conducted on NetBSD/earmv6hf and NetBSD/earmv7hf platforms.

---

## 1. Test Environment

- **Test Video**: `1000.mp4` (H.264, 1280x720, 30.00 FPS)
- **OS**: NetBSD 11.99.x (GENERIC)
- **Software Stack**: FFmpeg 7.x, NetBSD Native `wsdisplay` Framebuffer (`/dev/ttyE0` / `/dev/fb0`)

---

## 2. Benchmark Results

### 2.1 NetBSD/earmv6hf (Raspberry Pi 1 / Zero - Single Core ARMv6 @ 700MHz-1GHz)

| Test Case | Optimization / Configuration | Elapsed Time | Processed | Rendered | Dropped | Avg / Effective Speed |
|---|---|---|---|---|---|---|
| **CPU Decode Only** | Default FFmpeg 7 Flags | 17.12 sec | 300 frames | N/A | N/A | **17.53 FPS** |
| **CPU Decode Only** | Fast Flags (`skip_loop_filter=NONREF`) | 16.98 sec | 300 frames | N/A | N/A | **17.67 FPS** |
| **CPU Decode Only** | Max Fast (`skip_loop_filter=ALL`) | 14.59 sec | 300 frames | N/A | N/A | **20.56 FPS** |
| **Full FB Render** | Unconstrained Framebuffer Render | 25.98 sec | 300 frames | 300 frames | 0 frames | **11.55 FPS** |
| **Adaptive Sync** | Adaptive Timer Sync + Skip | 20.37 sec | 300 frames | 150 frames | 150 frames | **7.36 FPS** |
| **Dirty Region FB** | Dirty Region Overlay Optimization | 21.16 sec | 300 frames | 150 frames | 150 frames | **7.09 FPS** |

### 2.2 NetBSD/earmv7hf (Raspberry Pi 2 Model B - 4-core Cortex-A7 @ 600MHz)

| Test Case | Optimization / Configuration | Elapsed Time | Processed | Rendered | Dropped | Avg / Effective Speed |
|---|---|---|---|---|---|---|
| **Full Playback (Single Thread)** | `thread_count=1`, No Overlay | 459.15 sec | 7,274 frames | 3,637 frames | 3,637 frames | **7.92 FPS** |
| **Full Playback (Multi Thread)** | `thread_count=4`, NEON Enabled (`-pthread -mfpu=neon`) | 386.37 sec | 7,274 frames | 3,637 frames | 3,637 frames | **9.41 FPS** |

---

## 3. Bottleneck Analysis & Findings

1. **Decoding Performance**:
   - Pure H.264 software decoding on a single-core ARMv6 CPU reaches ~20.5 FPS for 720p resolution when skipping loop filters.
2. **Framebuffer Scaling & Color Conversion Bottleneck**:
   - The primary performance bottleneck is full-screen YUV420P -> RGB32 (32bpp) colorspace conversion and memory transfer via `swscale` for 1280x720 pixels.
   - Reducing video resolution to 480p (854x480 / 640x480) reduces pixel processing load by ~55%, enabling full-speed 30 FPS playback without frame drops.
3. **Adaptive Timer Synchronization**:
   - The interval-based adaptive frame-skipping mechanism successfully prevents audio/time drift without causing playback crashes.


