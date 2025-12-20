/*
 * VideoEncoder.cpp
 */
#include "VideoEncoder.h"
#include <stdio.h>
#include <string.h>

// Attempt to include VP9 header
#if __has_include(<vpx/vp9cx.h>)
#include <vpx/vp9cx.h>
#else
// Fallback declaration if header is missing but library has it
extern "C" vpx_codec_iface_t *vpx_codec_vp9_cx(void);
#endif

// Attempt to include AV1 header
#if __has_include(<aom/aomcx.h>)
#include <aom/aomcx.h>
#endif

VideoEncoder::VideoEncoder()
    : fVpxCfg(), fVpxImg(nullptr),fInitialized(false), fCodecName("vp8") {
    memset(&fCodec, 0, sizeof(fCodec));
}

VideoEncoder::~VideoEncoder() {
    if (fInitialized) vpx_codec_destroy(&fCodec);
    if (fVpxImg) vpx_img_free(fVpxImg);
}

status_t
VideoEncoder::Init(const int width, const int height, int32 bitrateKbps, const char *codec) {
    if (fInitialized) {
        vpx_codec_destroy(&fCodec);
        if (fVpxImg) vpx_img_free(fVpxImg);
        fVpxImg = nullptr;
        fInitialized = false;
    }

    fCodecName = codec;
    vpx_codec_iface_t *iface = nullptr;

    if (fCodecName == "vp8") {
        iface = vpx_codec_vp8_cx();
    } else if (fCodecName == "vp9") {
        iface = vpx_codec_vp9_cx();
    } else if (fCodecName == "av1") {
#if __has_include(<aom/aomcx.h>)
        // iface = aom_codec_av1_cx();
        fprintf(stderr, "AV1 support pending refactoring (vpx vs aom types).\n");
        return B_ERROR;
#else
        fprintf(stderr, "AV1 support not compiled in.\n");
        return B_ERROR;
#endif
    } else {
        fprintf(stderr, "VideoEncoder: Unsupported codec '%s' (Only vp8/vp9 supported)\n", codec);
        return B_ERROR;
    }

    if (!iface) {
        fprintf(stderr, "VideoEncoder: Failed to get codec interface for %s\n", codec);
        return B_ERROR;
    }

    if (const vpx_codec_err_t res = vpx_codec_enc_config_default(iface, &fVpxCfg, 0)) {
        fprintf(stderr, "Failed to get config: %s\n", vpx_codec_err_to_string(res));
        return B_ERROR;
    }

    fVpxCfg.g_w = width;
    fVpxCfg.g_h = height;
    fVpxCfg.rc_target_bitrate = bitrateKbps;
    fVpxCfg.g_timebase.num = 1;
    fVpxCfg.g_timebase.den = 30; // 30fps
    fVpxCfg.g_threads = 4; // Use multi-threading
    fVpxCfg.g_lag_in_frames = 0; // Low latency

    if (vpx_codec_enc_init(&fCodec, iface, &fVpxCfg, 0)) {
        fprintf(stderr, "Failed to init codec: %s\n", vpx_codec_error(&fCodec));
        return B_ERROR;
    }

    // Realtime settings
    vpx_codec_control(&fCodec, VP8E_SET_CPUUSED, 4);
    vpx_codec_control(&fCodec, VP8E_SET_STATIC_THRESHOLD, 1000);
    vpx_codec_control(&fCodec, VP8E_SET_NOISE_SENSITIVITY, 0);
    vpx_codec_control(&fCodec, VP8E_SET_TOKEN_PARTITIONS, 2);

    fVpxImg = vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, width, height, 1);
    if (!fVpxImg) return B_NO_MEMORY;

    fInitialized = true;
    return B_OK;
}

void
VideoEncoder::SetBitrate(int32 kbps) {
    if (!fInitialized) return;

    fVpxCfg.rc_target_bitrate = kbps;
    if (const vpx_codec_err_t res = vpx_codec_enc_config_set(&fCodec, &fVpxCfg)) {
        fprintf(stderr, "Failed to update bitrate: %s\n", vpx_codec_err_to_string(res));
    }
}

const char *
VideoEncoder::GetCodecName() const {
    return fCodecName.String();
}

status_t
VideoEncoder::Encode(const uint8 *bits, int32 stride, int64 pts, const bool forceKeyframe) {
    if (!fInitialized || !bits) return B_NO_INIT;

    // Convert RGB to YUV
    _RGBToYUV420(bits, stride, fVpxImg, fVpxImg->d_w, fVpxImg->d_h);

    // Encode
    vpx_codec_err_t res = vpx_codec_encode(&fCodec, fVpxImg, pts, 1, forceKeyframe ? VPX_EFLAG_FORCE_KF : 0,
                                           VPX_DL_REALTIME);

    return res == VPX_CODEC_OK ? B_OK : B_ERROR;
}

const vpx_codec_cx_pkt_t *
VideoEncoder::GetNextPacket(vpx_codec_iter_t *iter) {
    return vpx_codec_get_cx_data(&fCodec, iter);
}


// SSE4.1 Optimized RGB (B_RGB32) to YUV420
void
VideoEncoder::_RGBToYUV420(const uint8 *rgb, int32 stride, vpx_image_t *img, const int width, const int height) {
    uint8 *yPlane = img->planes[VPX_PLANE_Y];
    uint8 *uPlane = img->planes[VPX_PLANE_U];
    uint8 *vPlane = img->planes[VPX_PLANE_V];
    const int yStride = img->stride[VPX_PLANE_Y];
    const int uStride = img->stride[VPX_PLANE_U];
    const int vStride = img->stride[VPX_PLANE_V];

    const __m128i kYCoeffs = _mm_setr_epi16(25, 129, 66, 0, 25, 129, 66, 0);
    const __m128i kAdd128 = _mm_set1_epi32(128);
    const __m128i kAdd16 = _mm_set1_epi16(16);
    const __m128i kZero = _mm_setzero_si128();

    for (int y = 0; y < height; y++) {
        const uint8 *rowPtr = rgb + y * stride;
        uint8 *yDest = yPlane + y * yStride;

        int x = 0;
        // SIMD Y-Plane (Process 8 pixels at a time)
        for (; x < width - 7; x += 8) {
            __m128i px03 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(rowPtr + x * 4));
            __m128i px47 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(rowPtr + (x + 4) * 4));

            __m128i l03 = _mm_madd_epi16(_mm_unpacklo_epi8(px03, kZero), kYCoeffs);
            __m128i h03 = _mm_madd_epi16(_mm_unpackhi_epi8(px03, kZero), kYCoeffs);
            __m128i l47 = _mm_madd_epi16(_mm_unpacklo_epi8(px47, kZero), kYCoeffs);
            __m128i h47 = _mm_madd_epi16(_mm_unpackhi_epi8(px47, kZero), kYCoeffs);

            // y = (B*25 + G*129) + (R*66 + 0)
            __m128i y03 = _mm_hadd_epi32(l03, h03);
            __m128i y47 = _mm_hadd_epi32(l47, h47);

            y03 = _mm_srli_epi32(_mm_add_epi32(y03, kAdd128), 8);
            y47 = _mm_srli_epi32(_mm_add_epi32(y47, kAdd128), 8);

            __m128i y16 = _mm_add_epi16(_mm_packus_epi32(y03, y47), kAdd16);
            __m128i y8 = _mm_packus_epi16(y16, kZero);

            _mm_storel_epi64(reinterpret_cast<__m128i *>(yDest + x), y8);
        }

        // Fallback for Y and full UV
        for (; x < width; x++) {
            const uint8 *p = rowPtr + x * 4;
            int B = p[0];
            int G = p[1];
            int R = p[2];
            yDest[x] = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
        }

        if (y % 2 == 0) {
            uint8 *uDest = uPlane + y / 2 * uStride;
            uint8 *vDest = vPlane + y / 2 * vStride;
            for (int cx = 0; cx < width; cx += 2) {
                const uint8 *p = rowPtr + cx * 4;
                int B = p[0];
                int G = p[1];
                int R = p[2];
                uDest[cx / 2] = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
                vDest[cx / 2] = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
            }
        }
    }
}
