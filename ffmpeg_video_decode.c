#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

int video_decode_init(tFFContext *pFFCtx)
{
    AVFrame       *pAVFrame;
    AVCodec         *pAVCodec;
    AVCodecContext *avCodecContext;

    avcodec_register_all();
    av_register_all();


    pAVCodec = avcodec_find_decoder(pFFCtx->codec_id); //AV_CODEC_ID_H264

    if (pAVCodec == NULL) {
        printf("Codec not found\n");
        return RET_FAIL;
    }

    avCodecContext = avcodec_alloc_context3(pAVCodec);

    pAVFrame = av_frame_alloc();
    if (pFFCtx->extradata_size > 0) {
        avCodecContext->extradata = pFFCtx->extradata;
        avCodecContext->extradata_size = (int)pFFCtx->extradata_size;
    } else {
        avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
    }
    avCodecContext->flags |= CODEC_FLAG_TRUNCATED;

    if (avcodec_open2(avCodecContext, pAVCodec, NULL) < 0) {
        printf("Could not open codec\n");
        return RET_FAIL;
    }

    pFFCtx->avFrame = pAVFrame;
    pFFCtx->avCodec = pAVCodec;
    pFFCtx->avCodecContext = avCodecContext;

    return RET_SUCCESS;
}


int video_decode_deinit(tFFContext *pFFCtx)
{
    avcodec_close(pFFCtx->avCodecContext);
    if (pFFCtx->avCodecContext->priv_data) {
        avcodec_close (pFFCtx->avCodecContext);
    }
    if (pFFCtx->extradata_size == 0 && pFFCtx->avCodecContext->extradata) {
        av_free (pFFCtx->avCodecContext->extradata);
    }

    av_free (pFFCtx->avCodecContext);
    av_free(pFFCtx->avFrame);

    return RET_SUCCESS;
}


int decode_video(char *pFileName)
{
    int i, vRet = 0;
    FILE *pFileOut;
    size_t totalBytesRead = 0, bytesWrite = 0, totalBytesWrite = 0;

    // For file input
    AVFormatContext *pFormatCtxIn;
    AVPacket avPacketIn;
    int videoStream;

    // For file output
    tFFContext *pFFCtx = NULL;

    if (!pFileName) {
        perror("malloc fail..");
        return RET_FAIL;
    }

    pFFCtx = calloc(1, sizeof(tFFContext));
    if (NULL == pFFCtx) {
        perror("malloc fail..");
        return RET_FAIL;
    }


    /* Prepare input file, user should replace this section to network buffers */
    avcodec_register_all();
    av_register_all();
    av_init_packet(&avPacketIn);

    pFormatCtxIn = avformat_alloc_context();

    // http://www.live555.com/liveMedia/public/264/test.264
    // if(avformat_open_input(&pFormatCtxIn, "./test.264", NULL, NULL) != 0) {
    if (avformat_open_input(&pFormatCtxIn, pFileName, NULL, NULL) != 0) {
        perror("fopen failed in main");
        goto error1;
    }
    avformat_find_stream_info(pFormatCtxIn, NULL);
    av_dump_format(pFormatCtxIn, 0, pFileName, 0);
    printf("pFormatCtxIn->nb_streams = %d\n", pFormatCtxIn->nb_streams);

    for (i = 0; i < pFormatCtxIn->nb_streams; i++) {
        if (pFormatCtxIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            printf("videoStream=%d\n", videoStream);
            break;
        }
    }

    pFFCtx->extradata = pFormatCtxIn->streams[i]->codecpar->extradata;
    pFFCtx->extradata_size = pFormatCtxIn->streams[i]->codecpar->extradata_size;
    pFFCtx->codec_id = AV_CODEC_ID_H264;
    /* End of input prepare */


    video_decode_init(pFFCtx);


    /* Prepare output file */
    pFileOut = fopen("./output.yuv", "wb");
    if (NULL == pFileOut) {
        perror("fopen failed in main");
        vRet = RET_FAIL;
        goto error2;
    }

    /* Decode data */
    while (av_read_frame(pFormatCtxIn, &avPacketIn) >= 0) {
        if (avPacketIn.stream_index == videoStream) {
            totalBytesRead += avPacketIn.size;
            printf("avPacketIn.size=%d\n", avPacketIn.size);

            avcodec_send_packet(pFFCtx->avCodecContext, &avPacketIn);
            do {
                vRet = avcodec_receive_frame(pFFCtx->avCodecContext, pFFCtx->avFrame);
            } while (vRet == EAGAIN);

            if (vRet < 0) {
                /* if error, skip frame */
                break;
            } else {
                // https://fossies.org/linux/ffmpeg/doc/examples/decoding_encoding.c
                int bytes_num = av_image_get_buffer_size(pFFCtx->avFrame->format, pFFCtx->avFrame->width, pFFCtx->avFrame->height, 1);
                //printf("planes:%d linesize(%d,%d,%d)\n", av_pix_fmt_count_planes(pFFCtx->avFrame->format), pFFCtx->avFrame->linesize[0], pFFCtx->avFrame->linesize[1], pFFCtx->avFrame->linesize[2]);

                switch (pFFCtx->avFrame->format) {
                case AV_PIX_FMT_YUV420P:

                    // Test pFFCtx->avFrame->linesize[0]
                    // Y
                    vRet = fwrite(pFFCtx->avFrame->data[0], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->linesize[0], pFileOut);
                    bytesWrite = vRet;
                    // Cb
                    vRet = fwrite(pFFCtx->avFrame->data[1], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->linesize[1] / 2, pFileOut);
                    bytesWrite += vRet;
                    // Cr
                    vRet = fwrite(pFFCtx->avFrame->data[2], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->linesize[2] / 2, pFileOut);
                    bytesWrite += vRet;
                    break;
                case AV_PIX_FMT_YUV422P:
                    vRet = fwrite(pFFCtx->avFrame->data[0], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->width, pFileOut);
                    bytesWrite = vRet;
                    vRet = fwrite(pFFCtx->avFrame->data[1], 1, (pFFCtx->avFrame->height * pFFCtx->avFrame->width) / 2, pFileOut);
                    bytesWrite += vRet;
                    vRet = fwrite(pFFCtx->avFrame->data[2], 1, (pFFCtx->avFrame->height * pFFCtx->avFrame->width) / 2, pFileOut);
                    bytesWrite += vRet;
                    break;
                case AV_PIX_FMT_YUV444P16:
                    vRet = fwrite(pFFCtx->avFrame->data[0], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->width, pFileOut);
                    bytesWrite = vRet;
                    vRet = fwrite(pFFCtx->avFrame->data[1], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->width, pFileOut);
                    bytesWrite += vRet;
                    vRet = fwrite(pFFCtx->avFrame->data[2], 1, pFFCtx->avFrame->height * pFFCtx->avFrame->width, pFileOut);
                    bytesWrite += vRet;
                    break;

                default:
                    printf("unsupport format:%s, please contact me...\n", av_get_pix_fmt_name(pFFCtx->avFrame->format));
                }

                totalBytesWrite += bytesWrite;

                if (bytesWrite < bytes_num) {
                    printf("write file error %zu!=%d\n", bytesWrite, bytes_num);
                    break;
                }

                pFFCtx->frameCount++;
            }
        }
    }
    printf("totalBytesRead=%zu totalBytesWrite=%zu frameCount=%d\n", totalBytesRead, totalBytesWrite, pFFCtx->frameCount);


    video_decode_deinit(pFFCtx);


    /*Free and close everything*/
    fclose(pFileOut);

error2:
    avformat_close_input(&pFormatCtxIn);
error1:
    free(pFFCtx);
    return RET_SUCCESS;
}