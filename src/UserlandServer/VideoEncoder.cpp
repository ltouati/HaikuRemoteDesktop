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
extern "C" vpx_codec_iface_t* vpx_codec_vp9_cx(void);
#endif

// Attempt to include AV1 header
#if __has_include(<aom/aomcx.h>)
#include <aom/aomcx.h>
#endif

VideoEncoder::VideoEncoder()
	: fVpxImg(NULL), fInitialized(false), fCodecName("vp8")
{
	memset(&fCodec, 0, sizeof(fCodec));
}

VideoEncoder::~VideoEncoder()
{
	if (fInitialized) vpx_codec_destroy(&fCodec);
	if (fVpxImg) vpx_img_free(fVpxImg);
}

status_t 
VideoEncoder::Init(int width, int height, int32 bitrateKbps, const char* codec) 
{
	if (fInitialized) {
		vpx_codec_destroy(&fCodec);
		if (fVpxImg) vpx_img_free(fVpxImg);
		fVpxImg = NULL;
		fInitialized = false;
	}

	fCodecName = codec;
	vpx_codec_iface_t* iface = NULL;

	if (fCodecName == "vp8") {
		iface = vpx_codec_vp8_cx();
	} else if (fCodecName == "vp9") {
		iface = vpx_codec_vp9_cx();
	} else if (fCodecName == "av1") {
#if __has_include(<aom/aomcx.h>)
		// iface = aom_codec_av1_cx(); 
		// Note: libaom uses aom_codec_enc_cfg_t which is NOT vpx_codec_enc_cfg_t.
		// We cannot use fVpxCfg. We would need to refactor to use void* for config or template.
		// For now, we return error even if header exists, to avoid compilation error on types.
		// If we wanted to really support it, we'd need #ifdefs around fVpxCfg usage too.
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

	vpx_codec_err_t res = vpx_codec_enc_config_default(iface, &fVpxCfg, 0);
	if (res) {
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
	
	fVpxImg = vpx_img_alloc(NULL, VPX_IMG_FMT_I420, width, height, 1);
	if (!fVpxImg) return B_NO_MEMORY;

	fInitialized = true;
	return B_OK;
}

void 
VideoEncoder::SetBitrate(int32 kbps) 
{
	if (!fInitialized) return;
	
	fVpxCfg.rc_target_bitrate = kbps;
	vpx_codec_err_t res = vpx_codec_enc_config_set(&fCodec, &fVpxCfg);
	if (res) {
		fprintf(stderr, "Failed to update bitrate: %s\n", vpx_codec_err_to_string(res));
	}
}

const char* 
VideoEncoder::GetCodecName() const 
{
	return fCodecName.String();
}

status_t 
VideoEncoder::Encode(BBitmap* bitmap, int64 pts, bool forceKeyframe) 
{
	if (!fInitialized || !bitmap) return B_NO_INIT;

	// Convert RGB to YUV
	_RGBToYUV420((const uint8*)bitmap->Bits(), fVpxImg, fVpxImg->d_w, fVpxImg->d_h);

	// Encode
	vpx_codec_err_t res = vpx_codec_encode(&fCodec, fVpxImg, pts, 1, forceKeyframe ? VPX_EFLAG_FORCE_KF : 0, VPX_DL_REALTIME);
	
	return (res == VPX_CODEC_OK) ? B_OK : B_ERROR;
}

const vpx_codec_cx_pkt_t* 
VideoEncoder::GetNextPacket(vpx_codec_iter_t* iter) 
{
	return vpx_codec_get_cx_data(&fCodec, iter);
}

// SSE2 Optimized RGB to YUV420
void 
VideoEncoder::_RGBToYUV420(const uint8* rgb, vpx_image_t* img, int width, int height) 
{
	uint8* yPlane = img->planes[VPX_PLANE_Y];
	uint8* uPlane = img->planes[VPX_PLANE_U];
	uint8* vPlane = img->planes[VPX_PLANE_V];
	int yStride = img->stride[VPX_PLANE_Y];
	int uStride = img->stride[VPX_PLANE_U];
	int vStride = img->stride[VPX_PLANE_V];
	// Assume BBitmap is RGB32 (B G R A)
	int bytesPerRow = width * 4; 

	// Constants for Y conversion (Fixed point x256)
	const __m128i kYCoeffs = _mm_setr_epi16(25, 129, 66, 0, 25, 129, 66, 0);
	const __m128i kAdd128  = _mm_set1_epi16(128);
	const __m128i kAdd16   = _mm_set1_epi16(16);
	const __m128i kZero    = _mm_setzero_si128();

	for (int y = 0; y < height; y++) {
		const uint8* rowPtr = rgb + (y * bytesPerRow);
		uint8* yDest = yPlane + (y * yStride);
		
		int x = 0;
		// SIMD Y-Plane (Process 4 pixels at a time)
		for (; x < width - 3; x += 4) {
			__m128i px = _mm_loadu_si128((const __m128i*)(rowPtr + x * 4)); 
			
			// Unpack to 16-bit
			__m128i l = _mm_unpacklo_epi8(px, kZero); // P0, P1
			__m128i h = _mm_unpackhi_epi8(px, kZero); // P2, P3
			
			// Multiply by Coeffs
			l = _mm_mullo_epi16(l, kYCoeffs);
			h = _mm_mullo_epi16(h, kYCoeffs);
			
			// Horizontal Sum (B*25 + G*129 + R*66)
			// Slot 0 has B, Slot 1 has G, Slot 2 has R.
			l = _mm_add_epi16(l, _mm_srli_si128(l, 2)); // Add G to B position
			l = _mm_add_epi16(l, _mm_srli_si128(l, 4)); // Add R to B position
			
			h = _mm_add_epi16(h, _mm_srli_si128(h, 2));
			h = _mm_add_epi16(h, _mm_srli_si128(h, 4));
			
			// Add 128, Shift 8, Add 16
			l = _mm_add_epi16(l, kAdd128);
			l = _mm_srai_epi16(l, 8);
			l = _mm_add_epi16(l, kAdd16);
			
			h = _mm_add_epi16(h, kAdd128);
			h = _mm_srai_epi16(h, 8);
			h = _mm_add_epi16(h, kAdd16);
			
			// Extract and Store
			yDest[x+0] = (uint8)_mm_extract_epi16(l, 0);
			yDest[x+1] = (uint8)_mm_extract_epi16(l, 4);
			yDest[x+2] = (uint8)_mm_extract_epi16(h, 0);
			yDest[x+3] = (uint8)_mm_extract_epi16(h, 4);
		}
		
		// Handle remaining pixels (Scalar)
		for (; x < width; x++) {
			const uint8* p = rowPtr + x*4;
			int B = p[0]; int G = p[1]; int R = p[2];
			yDest[x] = ((66 * R + 129 * G +  25 * B + 128) >> 8) + 16;
		}

		// Subsample U/V (Every even row, even col)
		if (y % 2 == 0) {
			uint8* uDest = uPlane + ((y / 2) * uStride);
			uint8* vDest = vPlane + ((y / 2) * vStride);
			for (x = 0; x < width; x += 2) {
				const uint8* p = rowPtr + (x * 4);
				int B = p[0]; int G = p[1]; int R = p[2];
				
				uDest[x/2] = ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
				vDest[x/2] = ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
			}
		}
	}
}
