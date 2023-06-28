// #ifdef ENABLE_PRORES
#include "ProResDecoder.h"
#include "proresi.h"
#include "libavutil/pixfmt.h"

void initialize(ProResDecodeCodec* codec, AVStream* stream) {
    // codec->m_decoderContext->reset(PROpenDecoder(1, NULL), [](PRDecoder* ref) {
    //     if (ref) {
    //         PRCloseDecoder(ref);
    //     }
    // });
    codec->m_decoderContext = PROpenDecoder(1, NULL);
    codec->m_width = stream->codecpar->width;
    codec->m_height = stream->codecpar->height;
    return;
}

int initAppleProResDecoder(struct AVCodecContext* codec_context) {
    ProResDecodeCodec* codec;
    AVStream* stream;
    stream->codecpar->width = codec_context->width;
    stream->codecpar->height = codec_context->height;
    initialize(codec, stream);
    return 0;
}

int closeAppleProResDecoder(struct AVCodecContext* codec_context) {
    return 0;
}

int decodePacket(ProResDecodeCodec* codec, AVPacket* pkt, AVFrame* framePtr, bool drain) {
    if (drain) {
        // return AVError_create(AVErrorCode_kErrorEOF);
        return 1; // error code to be updated
    } else if (!pkt) {
        // return AVError_create(AVErrorCode_kErrorEAGAIN);
        return 1; // error code to be updated
    }
    int rowBytes = PRBytesPerRowNeededInPixelBuffer(codec->m_width, kPRFormat_2vuy, kPRDownscale_1x1);
    uint8_t* dataPtr = (uint8_t*)malloc(rowBytes * codec->m_height);
    if (dataPtr == NULL) {
        // return AVError_create(AVErrorCode_kMemoryAllocationFailed, "Failed to allocate memory for dataPtr");
        return 1; // error code to be updated
    }
    PRPixelBuffer destinationBuffer;
    destinationBuffer.baseAddr = dataPtr;
    destinationBuffer.rowBytes = rowBytes;
    destinationBuffer.format = kPRFormat_2vuy;
    destinationBuffer.width = codec->m_width;
    destinationBuffer.height = codec->m_height;
    int res = PRDecodeFrame(codec->m_decoderContext, pkt->data, pkt->size, &destinationBuffer, kPRDownscale_1x1, true);
    if (res < 0) {
        free(dataPtr);
        // return AVError_create(AVErrorCode_kDecoderReceiveFrameFailed, "Prores decoding failed");
        return 1; // error code to be updated
    }
    av_frame_unref(framePtr);
    framePtr->format = AV_PIX_FMT_YUV422P;
    framePtr->width  = codec->m_width;
    framePtr->height = codec->m_height;
    int ret = av_frame_get_buffer(framePtr, 0);
    if (ret < 0) {
        free(dataPtr);
        // return AVError_create(AVErrorCode_kMemoryAllocationFailed, "Failed to allocate memory for frame");
        return 1; // error code to be updated
    }
    memset(framePtr->data[0], 0, framePtr->linesize[0] * codec->m_height);
    memset(framePtr->data[1], 0, framePtr->linesize[1] * codec->m_height);
    memset(framePtr->data[2], 0, framePtr->linesize[2] * codec->m_height);
    
    // YUV422P data format order: U0 Y0 V0 Y1 U2 Y2 V2 Y3 U4 Y4 V4...
    for (int i = 0; i < codec->m_height; i++) {
        for (int j = 0; j < rowBytes; j++) {
            if (j % 4 == 0) { // U component
                memcpy(framePtr->data[1] + i * framePtr->linesize[1] + j / 4, dataPtr + i * rowBytes + j, 1);
            }
            if ((j - 1) % 2 == 0) { // Y component
                memcpy(framePtr->data[0] + i * framePtr->linesize[0] + (j - 1) / 2, dataPtr + i * rowBytes + j, 1);
            }
            if ((j - 2) % 4 == 0) { // V component.
                memcpy(framePtr->data[2] + i * framePtr->linesize[2] + (j - 2) / 4, dataPtr + i * rowBytes + j, 1);
            }
        }
    }
    return 0;
}

int decodeAppleProResFrame(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *avpkt) {
    ProResDecodeCodec* codec;
    return decodePacket(codec, avpkt, frame, 0);
}
// #endif