#include <proresdecoder.h>
#include "libproresdecoder.h"
#include "libavutil/pixfmt.h"
#include "thread.h"

int initAppleProResDecoder(struct AVCodecContext* avctx) {
    ProResDecodeCodec* proresdecodecctx = avctx->priv_data;
    proresdecodecctx->m_width = avctx->width; 
    proresdecodecctx->m_height = avctx->height; 
    // For now the downscale mode is universally 1x1
    proresdecodecctx->downscale_mode = kPRDownscale_1x1;
    
    // 4 letter tag signal the ProRes profiles. 
    switch (avctx->codec_tag) { 
    case MKTAG('a','p','c','o'):
        avctx->profile = FF_PROFILE_PRORES_PROXY;
        avctx->bits_per_raw_sample = 10;
        avctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        proresdecodecctx->pix_fmt = kPRFormat_v210;
        break;
    case MKTAG('a','p','c','s'):
        avctx->profile = FF_PROFILE_PRORES_LT;
        avctx->bits_per_raw_sample = 10;
        avctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        proresdecodecctx->pix_fmt = kPRFormat_v210;
        break;
    case MKTAG('a','p','c','n'):
        avctx->profile = FF_PROFILE_PRORES_STANDARD;
        avctx->bits_per_raw_sample = 10;
        avctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        proresdecodecctx->pix_fmt = kPRFormat_v210;
        break;
    case MKTAG('a','p','c','h'):
        avctx->profile = FF_PROFILE_PRORES_HQ;
        avctx->bits_per_raw_sample = 10;
        avctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        proresdecodecctx->pix_fmt = kPRFormat_v210;
        break;
    case MKTAG('a','p','4','h'):
        avctx->profile = FF_PROFILE_PRORES_4444;
        avctx->bits_per_raw_sample = 12;
        avctx->pix_fmt = AV_PIX_FMT_YUVA444P16LE;
        proresdecodecctx->pix_fmt = kPRFormat_y416;
        break;
    case MKTAG('a','p','4','x'):
        avctx->profile = FF_PROFILE_PRORES_XQ;
        avctx->bits_per_raw_sample = 12;
        avctx->pix_fmt = AV_PIX_FMT_YUVA444P16LE;
        proresdecodecctx->pix_fmt = kPRFormat_y416;
        break;
    default:
        avctx->profile = FF_PROFILE_UNKNOWN;
        av_log(avctx, AV_LOG_WARNING, "Unknown prores profile %d\n", avctx->codec_tag);
    }

    if (avctx->bits_per_raw_sample == 10) {
        av_log(avctx, AV_LOG_DEBUG, "Auto bitdepth precision. Use 10b decoding based on codec tag.\n");
    } else { /* 12b */
        av_log(avctx, AV_LOG_DEBUG, "Auto bitdepth precision. Use 12b decoding based on codec tag.\n");
    }

    proresdecodecctx->m_decoderContext = PROpenDecoder(1, NULL);
    return 0;
};

int decodeAppleProResFrame(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *avpkt) {
    ff_thread_finish_setup(avctx);
    *got_frame = 1; // Should this be set only for +ve return value?
    return decodePacket(avctx->priv_data, avpkt, frame, 0);
};

int decodePacket(ProResDecodeCodec* decoderctx, AVPacket* pkt, AVFrame* framePtr, bool drain) {
    if (drain) {
        // return AVError_create(AVErrorCode_kErrorEOF);
        return -1; // error code to be updated
    } else if (!pkt) {
        // return AVError_create(AVErrorCode_kErrorEAGAIN);
        return -2; // error code to be updated
    }

    int rowBytes = PRBytesPerRowNeededInPixelBuffer(decoderctx->m_width, decoderctx->pix_fmt, decoderctx->downscale_mode);
    uint8_t* dataPtr = (uint8_t*)malloc(rowBytes * decoderctx->m_height);
    if (dataPtr == NULL) {
        // return AVError_create(AVErrorCode_kMemoryAllocationFailed, "Failed to allocate memory for dataPtr");
        return -3; // error code to be updated
    }
    PRPixelBuffer destinationBuffer;
    destinationBuffer.baseAddr = dataPtr;
    destinationBuffer.rowBytes = rowBytes;
    destinationBuffer.format = decoderctx->pix_fmt;
    destinationBuffer.width = decoderctx->m_width;
    destinationBuffer.height = decoderctx->m_height;
    int res = PRDecodeFrame(decoderctx->m_decoderContext, pkt->data, pkt->size, &destinationBuffer, decoderctx->downscale_mode, false);
    if (res < 0) {
        free(dataPtr);
        // return AVError_create(AVErrorCode_kDecoderReceiveFrameFailed, "Prores decoding failed");
        return -4; // error code to be updated
    }

    // allocate planar YCbCr buffers (2B pixels)
    av_frame_unref(framePtr);
    framePtr->width  = decoderctx->m_width;
    framePtr->height = decoderctx->m_height;
    if (decoderctx->pix_fmt == kPRFormat_v210) {
        framePtr->format = AV_PIX_FMT_YUV422P10LE;
    } else if (decoderctx->pix_fmt == kPRFormat_v216) {
        framePtr->format = AV_PIX_FMT_YUV422P16LE;
    } else if (decoderctx->pix_fmt == kPRFormat_y416) {
        framePtr->format = AV_PIX_FMT_YUVA444P16LE;
    }
    int ret = av_frame_get_buffer(framePtr, 0);
    if (ret < 0) {
        free(dataPtr);
        // return AVError_create(AVErrorCode_kMemoryAllocationFailed, "Failed to allocate memory for frame");
        return -5; // error code to be updated
    }

    memset(framePtr->data[0], 0, framePtr->linesize[0] * decoderctx->m_height); // Y
    memset(framePtr->data[1], 0, framePtr->linesize[1] * decoderctx->m_height); // U
    memset(framePtr->data[2], 0, framePtr->linesize[2] * decoderctx->m_height); // V 
    
    if (decoderctx->pix_fmt == kPRFormat_v210) {
        // convert the v210 layout [16B packed data holds 6 YUV422 10b pixels] to YUV422p10LE (planar YCbCr) 
        // v210 [4W LE each] - W0[0b00|Cr0|Y0|Cb0] | W1[0b00|Y2|Cb1|Y1] | W2[0b00|Cb2|Y3|Cr1] | W3[0b00|Y5|Cr2|Y4]
        int dataUnitBytes = 2; // 10b pixel component stored in 16b (2B)
        int BytesInWord = 4;
        uint32_t W0, W1, W2, W3; // 4 words in the 16B data
        uint16_t Y0, Cb0, Cr0, Y1, Cb1, Cr1, Y2, Cb2, Cr2, Y3, Y4, Y5; // 16b intermediate pixel storage
        for (int i = 0; i < decoderctx->m_height; i++) {
            for (int j = 0, cj = 0, yj = 0; j < rowBytes; j=j+BytesInWord*4, cj=cj+3*dataUnitBytes, yj=yj+6*dataUnitBytes) {
                // Read the 4 LE words 
                memcpy(&W0, dataPtr + i * rowBytes + j, BytesInWord);
                memcpy(&W1, dataPtr + i * rowBytes + j + BytesInWord, BytesInWord);
                memcpy(&W2, dataPtr + i * rowBytes + j + 2*BytesInWord, BytesInWord);
                memcpy(&W3, dataPtr + i * rowBytes + j + 3*BytesInWord, BytesInWord);
                // Read from the v210 words into the intermediate pixel components
                Cb0 = W0 & 0x3ff;
                Y0 = W0 >> 10 & 0x3ff;
                Cr0 = W0 >> 20 & 0x3ff;
                Y1 = W1 & 0x3ff;
                Cb1 = W1 >> 10 & 0x3ff;
                Y2 = W1 >> 20 & 0x3ff;
                Cr1 = W2 & 0x3ff;
                Y3 = W2 >> 10 & 0x3ff;
                Cb2 = W2 >> 20 & 0x3ff;
                Y4 = W3 & 0x3ff;
                Cr2 = W3 >> 10 & 0x3ff;
                Y5 = W3 >> 20 & 0x3ff;
                // Copy the 10b pixel components into the respective planes - 6 Y, 3 Cb, 3 Cr (Total 6 pixels)
                memcpy(framePtr->data[1] + i * framePtr->linesize[1] + cj, &Cb0, dataUnitBytes); // Cb0
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj, &Y0, dataUnitBytes); // Y0
                memcpy(framePtr->data[2] + i * framePtr->linesize[2] + cj, &Cr0, dataUnitBytes); // Cr0
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj + dataUnitBytes, &Y1, dataUnitBytes); // Y1
                memcpy(framePtr->data[1] + i * framePtr->linesize[1] + cj + dataUnitBytes, &Cb1, dataUnitBytes); // Cb1
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj + 2*dataUnitBytes, &Y2, dataUnitBytes); // Y2
                memcpy(framePtr->data[2] + i * framePtr->linesize[2] + cj + dataUnitBytes, &Cr1, dataUnitBytes); // Cr1
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj + 3*dataUnitBytes, &Y3, dataUnitBytes); // Y3
                memcpy(framePtr->data[1] + i * framePtr->linesize[1] + cj + 2*dataUnitBytes, &Cb2, dataUnitBytes); // Cb2
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj + 4*dataUnitBytes, &Y4, dataUnitBytes); // Y4
                memcpy(framePtr->data[2] + i * framePtr->linesize[2] + cj + 2*dataUnitBytes, &Cr2, dataUnitBytes); // Cr2
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + yj + 5*dataUnitBytes, &Y5, dataUnitBytes); // Y5
            }
        }
    } else if (decoderctx->pix_fmt == kPRFormat_v216) {
        // convert the v216 layout [Cb0-2B, Y0-2B, Cr0-2B, Y1-2B] to YUV422p16LE (planar YCbCr) 
        int dataUnitBytes = 2; // read 2 bytes at a time
        for (int i = 0; i < decoderctx->m_height; i++) {
            for (int j = 0; j < rowBytes; j=j+dataUnitBytes) {
                if (j % (4*dataUnitBytes) == 0) { // U component
                    memcpy(framePtr->data[1] + i * framePtr->linesize[1] + (j / (4*dataUnitBytes))*dataUnitBytes, dataPtr + i * rowBytes + j, dataUnitBytes);
                }
                else if (j % (2*dataUnitBytes) == 0) { // V component.
                    memcpy(framePtr->data[2] + i * framePtr->linesize[2] + (j / (4*dataUnitBytes))*dataUnitBytes, dataPtr + i * rowBytes + j, dataUnitBytes);
                }
                else if (j % dataUnitBytes == 0) { // Y component
                    memcpy(framePtr->data[0] + i * framePtr->linesize[0] + (j / (2*dataUnitBytes))*dataUnitBytes, dataPtr + i * rowBytes + j, dataUnitBytes);
                }
            }
        }
    } else if (decoderctx->pix_fmt == kPRFormat_y416) {
        memset(framePtr->data[3], 0, framePtr->linesize[0] * decoderctx->m_height); // A
        // convert the y416 layout [A0 - 2B LE, Y0-2B LE, Cb0-2B LE, Cr0-2B LE] to YUVA444p16LE (planar YCbCrA) 
        int dataUnitBytes = 2; // read 2 bytes at a time
        for (int i = 0; i < decoderctx->m_height; i++) {
            for (int j = 0; j < rowBytes; j=j+dataUnitBytes) {
                int op_plane = ((j+6)%8)/2; // We want to right shift the AYCbCr order by 3 and round to get YCbCrA order
                int write_byte = (j / (4*dataUnitBytes))*dataUnitBytes; // 4 pixels = 4*dataUnitBytes bytes and write dataUnitBytes at a time
                memcpy(framePtr->data[op_plane] + i * framePtr->linesize[op_plane] + write_byte, dataPtr + i * rowBytes + j, dataUnitBytes);
            }
        }
    }
    free(dataPtr);
    return res;
};

#if HAVE_THREADS
static int update_thread_context(AVCodecContext *dstavctx, const AVCodecContext *srcavctx)
{
    ProResDecodeCodec *srcdecodectx = srcavctx->priv_data;
    ProResDecodeCodec *dstdecodectx = dstavctx->priv_data;
     
    dstavctx->profile = srcavctx->profile;
    dstavctx->bits_per_raw_sample = srcavctx->bits_per_raw_sample;
    dstavctx->pix_fmt = srcavctx->pix_fmt;

    dstdecodectx->m_width = srcdecodectx->m_width; 
    dstdecodectx->m_height = srcdecodectx->m_height;
    dstdecodectx->pix_fmt = srcdecodectx->pix_fmt;

    // For now the downscale mode is universally 1x1
    dstdecodectx->downscale_mode = kPRDownscale_1x1;

    return 0;
};
#endif

int closeAppleProResDecoder(struct AVCodecContext* avctx) {
    ProResDecodeCodec* proresdecodecctx = avctx->priv_data;
    PRCloseDecoder(proresdecodecctx->m_decoderContext);
    return 0;
};

const FFCodec ff_libproresdecoder_decoder = {
    .p.name         = "libproresdecoder",
    .p.long_name    = NULL_IF_CONFIG_SMALL("Apple ProRes (libProResDecoder.a)"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_PRORES,
    .priv_data_size = sizeof(ProResDecodeCodec),
    .init           = initAppleProResDecoder,
    .close          = closeAppleProResDecoder,
    FF_CODEC_DECODE_CB(decodeAppleProResFrame),
    .update_thread_context = ONLY_IF_THREADS_ENABLED(update_thread_context),
    .p.capabilities = AV_CODEC_CAP_FRAME_THREADS,
    .p.profiles     = NULL_IF_CONFIG_SMALL(ff_prores_profiles),
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
    .hw_configs     = NULL,
};