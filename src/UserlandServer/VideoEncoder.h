/*
 * VideoEncoder.h
 * Encapsulates VP9 Encoder and Color Conversion
 */
#ifndef VIDEO_ENCODER_H
#define VIDEO_ENCODER_H

#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
#include <Bitmap.h>
#include <Bitmap.h>
#include <emmintrin.h> // SSE2
#include <String.h>

class VideoEncoder {
public:
	VideoEncoder();
	~VideoEncoder();

	status_t Init(int width, int height, int32 bitrateKbps = 2000, const char* codec = "vp8");
	
	// Encodes the bitmap. Returns encoded packets via iterator.
	// Use GetNextPacket loop after calling Encode.
	status_t Encode(BBitmap* bitmap, int64 pts, bool forceKeyframe);
	
	const vpx_codec_cx_pkt_t* GetNextPacket(vpx_codec_iter_t* iter);
	
	void SetBitrate(int32 kbps);
	
	const char* GetCodecName() const;

private:
	vpx_codec_ctx_t fCodec;
	vpx_codec_enc_cfg_t fVpxCfg;
	vpx_image_t* fVpxImg;
	bool fInitialized;
	
	// SSE2 Color Conversion
	void _RGBToYUV420(const uint8* rgb, vpx_image_t* img, int width, int height);

	BString fCodecName;
};

#endif // VIDEO_ENCODER_H
