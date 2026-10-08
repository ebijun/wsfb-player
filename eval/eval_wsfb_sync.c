/*-
 * Copyright (c) 2026 Jun Ebihara 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/endian.h>
#include <time.h>

#include <dev/wscons/wsconsio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

typedef struct {
    int fd;
    uint8_t *map;
    size_t map_size;
    struct wsdisplay_fbinfo fbinfo;
    int stride;
} WSFBContext;

static int init_wsfb(WSFBContext *ws) {
    ws->fd = open("/dev/ttyE0", O_RDWR);
    if (ws->fd < 0) {
        ws->fd = open("/dev/fb0", O_RDWR);
    }
    if (ws->fd < 0) {
        perror("Failed to open /dev/ttyE0 or /dev/fb0");
        return -1;
    }

    if (ioctl(ws->fd, WSDISPLAYIO_GINFO, &ws->fbinfo) < 0) {
        perror("ioctl WSDISPLAYIO_GINFO failed");
        close(ws->fd);
        return -1;
    }

    int bytes_per_pixel = ws->fbinfo.depth / 8;
    if (bytes_per_pixel <= 0) bytes_per_pixel = 4;
    ws->stride = ws->fbinfo.width * bytes_per_pixel;

    int mode = WSDISPLAYIO_MODE_DUMBFB;
    if (ioctl(ws->fd, WSDISPLAYIO_SMODE, &mode) < 0) {
        perror("ioctl WSDISPLAYIO_SMODE failed");
        close(ws->fd);
        return -1;
    }

    ws->map_size = ws->stride * ws->fbinfo.height;
    ws->map = mmap(NULL, ws->map_size, PROT_READ | PROT_WRITE, MAP_SHARED, ws->fd, 0);
    if (ws->map == MAP_FAILED) {
        perror("mmap failed");
        mode = WSDISPLAYIO_MODE_EMUL;
        ioctl(ws->fd, WSDISPLAYIO_SMODE, &mode);
        close(ws->fd);
        return -1;
    }

    return 0;
}

static void cleanup_wsfb(WSFBContext *ws) {
    if (ws->map && ws->map != MAP_FAILED) {
        memset(ws->map, 0, ws->map_size);
        munmap(ws->map, ws->map_size);
    }
    if (ws->fd >= 0) {
        int mode = WSDISPLAYIO_MODE_EMUL;
        ioctl(ws->fd, WSDISPLAYIO_SMODE, &mode);
        close(ws->fd);
    }
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input_mp4_file>\n", argv[0]);
        return 1;
    }

    WSFBContext ws = { .fd = -1 };
    if (init_wsfb(&ws) < 0) {
        fprintf(stderr, "Failed to initialize NetBSD wsdisplay frame buffer\n");
        return 1;
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) return 1;
    avformat_find_stream_info(fmt_ctx, NULL);

    int video_stream = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = i;
            break;
        }
    }
    if (video_stream == -1) return 1;

    AVStream *stream = fmt_ctx->streams[video_stream];
    AVCodecParameters *codecpar = stream->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    codec_ctx->thread_count = 1;
    codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    codec_ctx->skip_loop_filter = AVDISCARD_ALL;

    avcodec_open2(codec_ctx, codec, NULL);

    double frame_delay = 1.0 / 30.0;
    if (stream->r_frame_rate.den > 0 && stream->r_frame_rate.num > 0) {
        frame_delay = av_q2d(av_inv_q(stream->r_frame_rate));
    }

#if BYTE_ORDER == BIG_ENDIAN
    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_0RGB;
#else
    enum AVPixelFormat dst_pix_fmt = AV_PIX_FMT_BGR0;
#endif

    struct SwsContext *sws_ctx = sws_getContext(
        codecpar->width, codecpar->height, codec_ctx->pix_fmt,
        ws.fbinfo.width, ws.fbinfo.height, dst_pix_fmt,
        SWS_POINT, NULL, NULL, NULL
    );

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    uint8_t *dst_data[4];
    int dst_linesize[4];
    dst_data[0] = ws.map;
    dst_linesize[0] = ws.stride;

    int rendered_frames = 0;
    int dropped_frames = 0;
    int processed_frames = 0;

    double start_time = get_time_sec();

    printf("=== Starting Video Playback with Adaptive Sync Test ===\n");

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    processed_frames++;

                    double target_time = processed_frames * frame_delay;
                    double actual_elapsed = get_time_sec() - start_time;
                    double delay = target_time - actual_elapsed;

                    if (delay > 0.001) {
                        usleep((useconds_t)(delay * 1e6));
                    }

                    if (delay < -0.066 && (processed_frames % 2 == 0)) {
                        dropped_frames++;
                        continue;
                    }

                    sws_scale(sws_ctx, (const uint8_t * const *)frame->data,
                              frame->linesize, 0, codecpar->height,
                              dst_data, dst_linesize);

                    rendered_frames++;

                    if (processed_frames >= 300) break;
                }
            }
        }
        av_packet_unref(pkt);
        if (processed_frames >= 300) break;
    }

    double total_elapsed = get_time_sec() - start_time;

    printf("\n=== Evaluation Results ===\n");
    printf("Total Processed : %d\n", processed_frames);
    printf("Rendered Frames : %d\n", rendered_frames);
    printf("Dropped Frames  : %d\n", dropped_frames);
    printf("Total Elapsed   : %.2f sec\n", total_elapsed);
    printf("Actual FPS      : %.2f FPS\n", rendered_frames / total_elapsed);

    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    cleanup_wsfb(&ws);

    return 0;
}


