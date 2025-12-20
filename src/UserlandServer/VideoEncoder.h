/*
 * VideoEncoder.h
 * Encapsulates VP9 Encoder and Color Conversion
 */
#ifndef VIDEO_ENCODER_H
#define VIDEO_ENCODER_H

#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
#include <smmintrin.h> // SSE4.1
#include <immintrin.h> // AVX
#include <String.h>

class VideoEncoder {
public:
    VideoEncoder();

    ~VideoEncoder();

    status_t Init(const int width, const int height, int32 bitrateKbps = 2000, const char *codec = "vp8");

    // Encodes raw RGB bits. Returns encoded packets via iterator.
    // Use GetNextPacket loop after calling Encode.
    status_t Encode(const uint8 *bits, int32 stride, int64 pts, const bool forceKeyframe);

    const vpx_codec_cx_pkt_t *GetNextPacket(vpx_codec_iter_t *iter);

    void SetBitrate(int32 kbps);

    const char *GetCodecName() const;

private:
    vpx_codec_ctx_t fCodec;
    vpx_codec_enc_cfg_t fVpxCfg;
    vpx_image_t *fVpxImg;
    bool fInitialized;

    // SIMD Color Conversion
    void _RGBToYUV420(const uint8 *rgb, int32 stride, vpx_image_t *img, const int width, const int height);

    // Manual Cursor Rendering
    void _DrawCursor(vpx_image_t *img, int x, int y);

    BString fCodecName;
};

#endif // VIDEO_ENCODER_H