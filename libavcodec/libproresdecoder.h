#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proresdecoder.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"

#include "libavutil/internal.h"
#include "libavutil/mem_internal.h"
#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "profiles.h"

typedef struct ProResDecodeCodec {
    PRDecoderRef m_decoderContext; 
    PRPixelFormat pix_fmt;
    PRDownscaleMode downscale_mode;
    int m_width;
    int m_height;
} ProResDecodeCodec;

/**
 * Initialize prores codec with given stream codec parameters.
 * @param AVStream pointer to media stream.
*/
void initialize(ProResDecodeCodec* codec, struct AVStream* stream);

int initAppleProResDecoder(struct AVCodecContext* codec_context);

int closeAppleProResDecoder(struct AVCodecContext* codec_context);

int decodeAppleProResFrame(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *avpkt);
   
/**
 * Decode packet to frame.
 * @param pkt AVPacket to be decoded.
 * @param frame Output of decoded packet.
 * @param drain true if draining.
 */
int decodePacket(ProResDecodeCodec* codec, struct AVPacket* pkt, struct AVFrame* frame, bool drain);

#if HAVE_THREADS
static int update_libproresdecoder_thread_context(AVCodecContext *dst, const AVCodecContext *src)
{
    return 0;
}
#endif
