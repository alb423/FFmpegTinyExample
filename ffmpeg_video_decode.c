#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

int video_decode_init(tFFContext *pFFCtx, char *pFileName, int *pVideoStreamId)
{
    int i, vRet = 0;
    AVCodec        *pAVCodec;
    AVCodecContext *avCodecContext;
    AVFormatContext *pFormatCtxIn;

    avcodec_register_all();
    av_register_all();

    do {
        pFormatCtxIn = avformat_alloc_context();
        if ((vRet = avformat_open_input(&pFormatCtxIn, pFileName, NULL, NULL)) != 0) {
            fprintf(stderr, "avformat_open_input fail (%s)\n", av_err2str(vRet));
            break;
        }
        avformat_find_stream_info(pFormatCtxIn, NULL);
        av_dump_format(pFormatCtxIn, 0, pFileName, 0);

        for (i = 0; i < pFormatCtxIn->nb_streams; i++) {
            if (pFormatCtxIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                *pVideoStreamId = i;
                printf("VideoStreamId=%d\n", *pVideoStreamId);
                break;
            }
        }

        pFFCtx->codec_id = pFormatCtxIn->streams[i]->codecpar->codec_id;

        pAVCodec = avcodec_find_decoder(pFFCtx->codec_id);
        if (pAVCodec == NULL) {
            printf("Codec not found\n");
            break;
        }

        avCodecContext = avcodec_alloc_context3(pAVCodec);

        pFFCtx->avFrame = av_frame_alloc();

        if (pFFCtx->extradata_size > 0) {
            avCodecContext->extradata = pFFCtx->extradata;
            avCodecContext->extradata_size = (int)pFFCtx->extradata_size;
        } else {
            avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
        }
        avCodecContext->flags |= CODEC_FLAG_TRUNCATED;

        vRet = avcodec_parameters_to_context(avCodecContext, pFormatCtxIn->streams[0]->codecpar);
        if (vRet < 0) {
            avformat_close_input(&pFormatCtxIn);
            avcodec_free_context(&avCodecContext);
            break;
        }

        if (avcodec_open2(avCodecContext, pAVCodec, NULL) < 0) {
            printf("Could not open codec(%s)\n", avcodec_get_name(pFFCtx->codec_id));
            break;
        }

        pFFCtx->avCodec = pAVCodec;
        pFFCtx->avCodecContext = avCodecContext;
        pFFCtx->pFormatCtxIn = pFormatCtxIn;

        return RET_SUCCESS;
    } while (0);

    if (pFormatCtxIn) {
        avformat_close_input(&pFormatCtxIn);
        avformat_free_context(pFormatCtxIn);
    }
    return RET_FAIL;
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

int decode_video(char *pFileName, enum AVPixelFormat *pPixFmt, int *pWidth, int *pHeight)
{
    int vRet = 0;
    FILE *pFileOut;
    size_t totalBytesRead = 0, bytesWrite = 0, totalBytesWrite = 0;

    AVFormatContext *pFormatCtxIn;
    AVPacket avPacketIn;
    int videoStream;

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

    avcodec_register_all();
    av_register_all();

    video_decode_init(pFFCtx, pFileName, &videoStream);
    *pWidth = pFFCtx->avCodecContext->coded_width;
    *pHeight = pFFCtx->avCodecContext->coded_height;
    *pPixFmt = pFFCtx->avCodecContext->pix_fmt;

    av_init_packet(&avPacketIn);

    /* Prepare output file */
    pFileOut = fopen("./output.yuv", "wb");
    if (NULL == pFileOut) {
        perror("fopen failed in main");
        vRet = RET_FAIL;
        goto error2;
    }

    /* Decode data */
    while (av_read_frame(pFFCtx->pFormatCtxIn, &avPacketIn) >= 0) {
        if (avPacketIn.stream_index == videoStream) {
            totalBytesRead += avPacketIn.size;
            vRet = avcodec_send_packet(pFFCtx->avCodecContext, &avPacketIn);
            if (vRet < 0 || vRet == AVERROR(EAGAIN) || vRet == AVERROR_EOF) {
                printf("avcodec_send_packet() error(%d) !! %s \n", AVERROR(vRet), av_err2str(vRet));
                break;
            }

            vRet = avcodec_receive_frame(pFFCtx->avCodecContext, pFFCtx->avFrame);
            if (vRet < 0 || vRet == AVERROR(EAGAIN) || vRet == AVERROR_EOF) {
                printf("avcodec_receive_frame() error(%d) !! %s \n\n", AVERROR(vRet), av_err2str(vRet));
                break;
            } else {
                // https://fossies.org/linux/ffmpeg/doc/examples/decoding_encoding.c
                int bytes_num = av_image_get_buffer_size(pFFCtx->avFrame->format, pFFCtx->avFrame->width, pFFCtx->avFrame->height, 1);
                //printf("planes:%d linesize(%d,%d,%d)\n", av_pix_fmt_count_planes(pFFCtx->avFrame->format), pFFCtx->avFrame->linesize[0], pFFCtx->avFrame->linesize[1], pFFCtx->avFrame->linesize[2]);

                switch (pFFCtx->avFrame->format) {
                case AV_PIX_FMT_YUV420P:
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
    free(pFFCtx);
    return RET_SUCCESS;
}