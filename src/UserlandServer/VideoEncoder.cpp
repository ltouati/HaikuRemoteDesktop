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
    : fVpxCfg(), fVpxImg(nullptr), fX264Codec(nullptr), fNals(nullptr), fNalCount(0), fInitialized(false),
      fCodecName("vp8") {
    memset(&fCodec, 0, sizeof(fCodec));
    memset(&fX264Param, 0, sizeof(fX264Param));
    memset(&fX264PicIn, 0, sizeof(fX264PicIn));
    memset(&fX264PicOut, 0, sizeof(fX264PicOut));
}

VideoEncoder::~VideoEncoder() {
    if (fInitialized) {
        if (fX264Codec) {
            x264_encoder_close(fX264Codec);
            x264_picture_clean(&fX264PicIn);
        } else {
            vpx_codec_destroy(&fCodec);
            if (fVpxImg) vpx_img_free(fVpxImg);
        }
    }
}

status_t
VideoEncoder::Init(const int width, const int height, int32 bitrateKbps, const char *codec) {
    if (fInitialized) {
        if (fX264Codec) {
            x264_encoder_close(fX264Codec);
            x264_picture_clean(&fX264PicIn);
            fX264Codec = nullptr;
        } else {
            vpx_codec_destroy(&fCodec);
            if (fVpxImg) vpx_img_free(fVpxImg);
            fVpxImg = nullptr;
        }
        fInitialized = false;
    }

    fCodecName = codec;

    if (fCodecName == "h264") {
        x264_param_default_preset(&fX264Param, "ultrafast", "zerolatency");
        fX264Param.i_width = width;
        fX264Param.i_height = height;
        fX264Param.i_fps_num = 30;
        fX264Param.i_fps_den = 1;
        fX264Param.i_keyint_max = 30; // 1 second?
        fX264Param.b_intra_refresh = 1; 
        fX264Param.rc.i_rc_method = X264_RC_ABR;
        fX264Param.rc.i_bitrate = bitrateKbps;
        fX264Param.b_repeat_headers = 1; // Annex B need headers for random access resilience
        
        // Profile
        x264_param_apply_profile(&fX264Param, "baseline");

        if (x264_picture_alloc(&fX264PicIn, X264_CSP_I420, width, height) < 0) {
            return B_NO_MEMORY;
        }

        fX264Codec = x264_encoder_open(&fX264Param);
        if (!fX264Codec) {
             x264_picture_clean(&fX264PicIn);
             return B_ERROR;
        }

        fInitialized = true;
        return B_OK;
    }

    vpx_codec_iface_t *iface = nullptr;

    if (fCodecName == "vp8") {
        iface = vpx_codec_vp8_cx();
    } else if (fCodecName == "vp9") {
        iface = vpx_codec_vp9_cx();
    } else {
        fprintf(stderr, "VideoEncoder: Unsupported codec '%s'\n", codec);
        return B_ERROR;
    }

    // ... VPX Init Logic (Preserved) ...
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
    if (fCodecName == "vp8") {
        vpx_codec_control(&fCodec, VP8E_SET_CPUUSED, 4);
        vpx_codec_control(&fCodec, VP8E_SET_STATIC_THRESHOLD, 1000);
        vpx_codec_control(&fCodec, VP8E_SET_NOISE_SENSITIVITY, 0);
        vpx_codec_control(&fCodec, VP8E_SET_TOKEN_PARTITIONS, 2);
    } else {
        // VP9 specifics if needed
        vpx_codec_control(&fCodec, VP8E_SET_CPUUSED, 4);
    }

    fVpxImg = vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, width, height, 1);
    if (!fVpxImg) return B_NO_MEMORY;

    fInitialized = true;
    return B_OK;
}

void
VideoEncoder::SetBitrate(int32 kbps) {
    if (!fInitialized) return;

    if (fX264Codec) {
        fX264Param.rc.i_bitrate = kbps;
        x264_encoder_reconfig(fX264Codec, &fX264Param);
        return;
    }

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

    if (fX264Codec) {
        // Assume resolution matches Init dimensions
        
        // Color Conversion
        _RGBToYUV420_X264(bits, stride, &fX264PicIn, fX264Param.i_width, fX264Param.i_height);
        
        fX264PicIn.i_pts = pts;
        fX264PicIn.i_type = forceKeyframe ? X264_TYPE_IDR : X264_TYPE_AUTO;

        int frameSize = x264_encoder_encode(fX264Codec, &fNals, &fNalCount, &fX264PicIn, &fX264PicOut);
        
        if (frameSize < 0) return B_ERROR;

        if (frameSize > 0) {
            fFakePkt.kind = VPX_CODEC_CX_FRAME_PKT;
            fFakePkt.data.frame.buf = fNals[0].p_payload;
            fFakePkt.data.frame.sz = frameSize;
            fFakePkt.data.frame.pts = fX264PicOut.i_pts;
            fFakePkt.data.frame.duration = 1;
            fFakePkt.data.frame.flags = fX264PicOut.b_keyframe ? VPX_FRAME_IS_KEY : 0;
            
            fCurrentNal = 1; // Unread packet available
        } else {
            fCurrentNal = 0;
        }

        return B_OK; 
    }

    // Convert RGB to YUV
    _RGBToYUV420(bits, stride, fVpxImg, fVpxImg->d_w, fVpxImg->d_h);

    // Encode
    vpx_codec_err_t res = vpx_codec_encode(&fCodec, fVpxImg, pts, 1, forceKeyframe ? VPX_EFLAG_FORCE_KF : 0,
                                           VPX_DL_REALTIME);

    return res == VPX_CODEC_OK ? B_OK : B_ERROR;
}

const vpx_codec_cx_pkt_t *
VideoEncoder::GetNextPacket(vpx_codec_iter_t *iter) {
    if (fX264Codec) {
        if (fCurrentNal == 1) {
             fCurrentNal = 0;
             return &fFakePkt;
        }
        return nullptr;
    }

    return vpx_codec_get_cx_data(&fCodec, iter);
}

bool
VideoEncoder::GetExtraData(uint8 **data, size_t *size) {
    // Not strictly needed if we assume ANNEX B (in-band headers)
    // x264 with b_repeat_headers=1 sends SPS/PPS before every keyframe.
    return false;
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

// Same as above but for x264_picture_t
void
VideoEncoder::_RGBToYUV420_X264(const uint8 *rgb, int32 stride, x264_picture_t *pic, const int width, const int height) {
    uint8 *yPlane = pic->img.plane[0];
    uint8 *uPlane = pic->img.plane[1];
    uint8 *vPlane = pic->img.plane[2];
    const int yStride = pic->img.i_stride[0];
    const int uStride = pic->img.i_stride[1];
    const int vStride = pic->img.i_stride[2];

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
