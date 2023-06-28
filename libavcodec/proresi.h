#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ProResDecoder.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"

#include "libavutil/internal.h"
#include "libavutil/mem_internal.h"
#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "profiles.h"

// struct PRDecoder;

typedef struct ProResDecodeCodec {
    PRDecoderRef m_decoderContext; 
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
static int update_apple_prores_thread_context(AVCodecContext *dst, const AVCodecContext *src)
{
    return 0;
}
#endif

// const FFCodec ff_apple_prores_decoder = {
//     .p.name         = "apple_prores",
//     CODEC_LONG_NAME("Apple ProRes (native)"),
//     .p.type         = AVMEDIA_TYPE_VIDEO,
//     .p.id           = AV_CODEC_ID_APPLE_PRORES,
//     .priv_data_size = sizeof(ProResDecodeCodec),
//     .init           = initAppleProResDecoder,
//     .close          = closeAppleProResDecoder,
//     FF_CODEC_DECODE_CB(decodeAppleProResFrame),
//     UPDATE_THREAD_CONTEXT(update_apple_prores_thread_context),
//     .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS | AV_CODEC_CAP_FRAME_THREADS,
//     .p.profiles     = NULL_IF_CONFIG_SMALL(ff_prores_profiles),
//     .hw_configs     = NULL,
// };
