#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "ffmpeg_decode.h"

#define DST_SAMPLE_FMT       AV_SAMPLE_FMT_S16
// To support tinyalsa
#if 1
#define DST_SAMPLE_RATE      48000
#define DST_CHANNEL          2
#define DST_CHANNEL_LAYOUT   AV_CH_LAYOUT_STEREO
#else
#define DST_SAMPLE_RATE      16000
#define DST_CHANNEL          1
#define DST_CHANNEL_LAYOUT   AV_CH_LAYOUT_MONO
#endif

#if RENDER_BY_TINYALSA==1
#include <tinyalsa/asoundlib.h>
struct Cmd {
    unsigned int card;
    unsigned int device;
    int flags;
    struct pcm_config config;
    unsigned int bits;
};
struct Ctx {
    struct pcm *pcm;
};
struct Ctx *pTinyCtx;
#endif

int audio_decode_init(tFFContext *pFFCtx, char *pFileName, int *pAudioStreamId)
{
    int i, vRet = 0;
    AVCodec        *pAVCodec;
    AVCodecContext *avCodecContext;
    SwrContext     *swr;
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
            if (pFormatCtxIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                *pAudioStreamId = i;
                //printf("audioStream=%d\n", *pAudioStreamId);
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

        if ( (avCodecContext->channel_layout != DST_CHANNEL_LAYOUT) ||
             (avCodecContext->sample_rate != DST_SAMPLE_RATE) ||
             (avCodecContext->sample_fmt != DST_SAMPLE_FMT)) {
            swr = swr_alloc();
            av_opt_set_int(swr, "in_channel_layout",  avCodecContext->channel_layout, 0);
            av_opt_set_int(swr, "out_channel_layout", DST_CHANNEL_LAYOUT,  0);
            av_opt_set_int(swr, "in_sample_rate",     avCodecContext->sample_rate, 0);
            av_opt_set_int(swr, "out_sample_rate",    DST_SAMPLE_RATE, 0);
            av_opt_set_sample_fmt(swr, "in_sample_fmt",  avCodecContext->sample_fmt, 0);
            av_opt_set_sample_fmt(swr, "out_sample_fmt", DST_SAMPLE_FMT,  0);
            swr_init(swr);
        }

        pFFCtx->avCodec = pAVCodec;
        pFFCtx->avCodecContext = avCodecContext;
        pFFCtx->pFormatCtxIn = pFormatCtxIn;
        pFFCtx->swr = swr;

        return RET_SUCCESS;
    } while (0);

    if (pFormatCtxIn) {
        avformat_close_input(&pFormatCtxIn);
        avformat_free_context(pFormatCtxIn);
    }
    return RET_FAIL;
}


int audio_decode_deinit(tFFContext *pFFCtx)
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


int decode_audio(char *pFileName, enum AVSampleFormat *pSampleFmt, int *pSampleRate, int *pChannels)
{
    int vRet = 0, bytes_num = 0;
    FILE *pFileOut;
    size_t totalBytesRead = 0, totalBytesWrite = 0;
    int dst_linesize, max_dst_nb_samples, dst_nb_samples;

    AVPacket avPacketIn;
    int audioStream = -1;

    uint8_t **ppOutputBuffer = NULL;

    tFFContext *pFFCtx = NULL;

    if (!pFileName) {
        perror("unknow decode file");
        return RET_FAIL;
    }

    pFFCtx = calloc(1, sizeof(tFFContext));
    if (NULL == pFFCtx) {
        perror("malloc fail..");
        return RET_FAIL;
    }

    audio_decode_init(pFFCtx, pFileName, &audioStream);
    *pSampleFmt   = DST_SAMPLE_FMT;
    *pSampleRate  = DST_SAMPLE_RATE;
    *pChannels    = DST_CHANNEL;

    av_init_packet(&avPacketIn);

#if RENDER_BY_TINYALSA==1
    struct Cmd vxCmd = {0};
    pTinyCtx = (struct Ctx *)malloc(sizeof(struct Ctx));
    vxCmd.card = 0;
    vxCmd.device = 0;
    vxCmd.flags = PCM_OUT;
    vxCmd.config.period_size = 1024; //1024
    vxCmd.config.period_count = 2;
    vxCmd.config.channels = DST_CHANNEL; //2,
    vxCmd.config.rate = DST_SAMPLE_RATE; //48000,
    vxCmd.config.format = PCM_FORMAT_S16_LE;
    vxCmd.config.silence_threshold = 1024 * 2;
    vxCmd.config.stop_threshold = 1024 * 2;
    vxCmd.config.start_threshold = 1024;
    vxCmd.bits = 16;

    pTinyCtx->pcm = pcm_open(vxCmd.card, vxCmd.device, vxCmd.flags, &vxCmd.config);
    if (pTinyCtx->pcm == NULL) {
        fprintf(stderr, "failed to allocate memory for pcm\n");
        goto error2;
    } else if (!pcm_is_ready(pTinyCtx->pcm)) {
        fprintf(stderr, "failed to open pcm(%p) %u,%u\n", pTinyCtx->pcm, vxCmd.card, vxCmd.device);
        goto error2;
    }
    printf("pTinyCtx=%p, pTinyCtx->pcm=%p\n", pTinyCtx, pTinyCtx->pcm);
#endif

    pFileOut = fopen("./output.pcm", "wb");
    if (NULL == pFileOut) {
        printf("fopen failed \n");
        vRet = RET_FAIL;
        goto error2;
    }

    max_dst_nb_samples = dst_nb_samples =
                             av_rescale_rnd(1024, DST_SAMPLE_RATE, pFFCtx->avCodecContext->sample_rate, AV_ROUND_UP);
    vRet = av_samples_alloc_array_and_samples(&ppOutputBuffer, &dst_linesize, DST_CHANNEL, dst_nb_samples, DST_SAMPLE_FMT, 0);
    if (vRet < 0) {
        printf("av_samples_alloc_array_and_samples() error!!\n");
        vRet = RET_FAIL;
        goto error2;
    }

    /* Decode data */
    while (av_read_frame(pFFCtx->pFormatCtxIn, &avPacketIn) >= 0) {
        if (avPacketIn.stream_index == audioStream) {
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
                dst_nb_samples = av_rescale_rnd(swr_get_delay(pFFCtx->swr, pFFCtx->avCodecContext->sample_rate) + pFFCtx->avFrame->nb_samples,
                                                DST_SAMPLE_RATE,
                                                pFFCtx->avCodecContext->sample_rate,
                                                AV_ROUND_UP);
                if (dst_nb_samples > max_dst_nb_samples) {
                    av_freep(&ppOutputBuffer[0]);
                    vRet = av_samples_alloc(ppOutputBuffer, &dst_linesize, DST_CHANNEL,
                                            dst_nb_samples, DST_SAMPLE_FMT, 1);
                    if (vRet < 0) {
                        printf("av_samples_alloc() error !!\n");
                        break;
                    }
                    max_dst_nb_samples = dst_nb_samples;
                }


                if (pFFCtx->avCodecContext->sample_fmt != DST_SAMPLE_FMT) {
                    vRet = swr_convert(pFFCtx->swr,
                                       ppOutputBuffer,
                                       pFFCtx->avFrame->nb_samples,
                                       (const uint8_t **)pFFCtx->avFrame->data,
                                       pFFCtx->avFrame->nb_samples);
                    bytes_num = av_samples_get_buffer_size(NULL, DST_CHANNEL, vRet, DST_SAMPLE_FMT, 1);
                    vRet = fwrite(ppOutputBuffer[0], 1, bytes_num, pFileOut);
                    if (vRet < bytes_num) {
                        printf("write file error %d!=%d\n", bytes_num, vRet);
                        break;
                    }
                } else {
                    bytes_num = av_samples_get_buffer_size(NULL,
                                                           pFFCtx->avFrame->channels,
                                                           pFFCtx->avFrame->nb_samples,
                                                           pFFCtx->avCodecContext->sample_fmt,
                                                           1);
                    vRet = fwrite(pFFCtx->avFrame->data[0], 1, bytes_num, pFileOut);
                    if (vRet < bytes_num) {
                        printf("write file error %d!=%d\n", bytes_num, vRet);
                        break;
                    }
                }


                if (vRet > 0) {
#if RENDER_BY_TINYALSA==1
                    if (pcm_writei(pTinyCtx->pcm, ppOutputBuffer[0], bytes_num) < 0) {
                        fprintf(stderr, "error playing sample\n");
                    }
                    // DST_SAMPLE_RATE / 1000 / channels / bytesPerSample = 48000/(1024*2**2) = 11.719
                    usleep(11719);
#endif
                    totalBytesWrite += bytes_num;
                }
            }
        }
    }

#if 0
    // Flushing, enter draining mode
    vRet = avcodec_send_packet(pFFCtx->avCodecContext, NULL);
    if (vRet < 0 || vRet == AVERROR(EAGAIN) || vRet == AVERROR_EOF) {
        printf("avcodec_send_packet() error(%d) !! %s \n", AVERROR(vRet), av_err2str(vRet));
    } else {
        do {
            vRet = avcodec_receive_frame(pFFCtx->avCodecContext, pFFCtx->avFrame);
            if (vRet > 0) {

                if (pFFCtx->avCodecContext->sample_fmt != DST_SAMPLE_FMT) {
                    vRet = swr_convert(pFFCtx->swr,
                                       ppOutputBuffer,
                                       pFFCtx->avFrame->nb_samples,
                                       (const uint8_t **)pFFCtx->avFrame->data,
                                       pFFCtx->avFrame->nb_samples);
                    bytes_num = av_samples_get_buffer_size(NULL, DST_CHANNEL, vRet, DST_SAMPLE_FMT, 1);
                    vRet = fwrite(ppOutputBuffer[0], 1, bytes_num, pFileOut);
                    if (vRet < bytes_num) {
                        printf("write file error %d!=%d\n", bytes_num, vRet);
                        break;
                    }
                } else {
                    bytes_num = av_samples_get_buffer_size(NULL,
                                                           pFFCtx->avFrame->channels,
                                                           pFFCtx->avFrame->nb_samples,
                                                           pFFCtx->avCodecContext->sample_fmt,
                                                           1);
                    vRet = fwrite(pFFCtx->avFrame->data[0], 1, bytes_num, pFileOut);
                    if (vRet < bytes_num) {
                        printf("write file error %d!=%d\n", bytes_num, vRet);
                        break;
                    }
                }
                if (vRet > 0)
                    totalBytesWrite += bytes_num;
            }
        } while (vRet == AVERROR(EAGAIN));
    }
#endif
    printf("totalBytesRead=%zu totalBytesWrite=%zu\n", totalBytesRead, totalBytesWrite);

    /*Free and close everything*/
    fclose(pFileOut);
    if (ppOutputBuffer)
        av_freep(&ppOutputBuffer[0]);
    av_freep(&ppOutputBuffer);

error2:
#if RENDER_BY_TINYALSA==1
    if (pTinyCtx == NULL) {
        return RET_SUCCESS;
    }
    if (pTinyCtx->pcm != NULL) {
        pcm_close(pTinyCtx->pcm);
    }
    free(pTinyCtx);
#endif

    audio_decode_deinit(pFFCtx);
    avformat_close_input(&pFFCtx->pFormatCtxIn);
    free(pFFCtx);
    if (vRet == RET_FAIL)
        return RET_FAIL;
    else
        return RET_SUCCESS;
}