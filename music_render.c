// Usage
// if RENDER_BY_FFMPEG == 1
// 		./music_render ../sample/sample_audio.mp3
// if RENDER_BY_FFMPEG == 0
// 		./music_render ../sample/sample_audio.wav

#define RENDER_BY_FFMPEG	1 // 0 or 1
#define DELAY_TIME		  100 // For wave, delay 50

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <tinyalsa/asoundlib.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "music_render.h"
#define BUF_SIZE 4800
// move from ffmpeg_audio_decode.c
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>


#define DST_SAMPLE_FMT       AV_SAMPLE_FMT_S16
#define DST_SAMPLE_RATE      48000
#define DST_CHANNEL          2
#define DST_CHANNEL_LAYOUT   AV_CH_LAYOUT_STEREO

static int _gExit = 0;


// Local Data Structure
struct Cmd {
    unsigned int card;
    unsigned int device;
    int flags;
    struct pcm_config config;
    unsigned int bits;
};


#if RENDER_BY_FFMPEG == 1
typedef struct tFFContext {
    AVFrame         *avFrame;
    AVCodec         *avCodec;
    AVCodecContext  *avCodecContext;
    AVFormatContext *pFormatCtxIn;
    SwrContext      *swr;
    int             frameCount;
    int             codec_id;
    int             extradata_size;
    unsigned char   *extradata;
} tFFContext;

/// External Function
extern void avcodec_register_all(void);
extern int avcodec_open2 (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
extern AVCodecContext * avcodec_alloc_context3(const AVCodec *codec);
extern AVFrame * av_frame_alloc(void);

// Local Functionss
int audio_decode_init(tFFContext *pFFCtx, char *pFileName, int *pAudioStreamId);
int audio_decode_deinit(tFFContext *pFFCtx);

#endif
//static int kConversionbufferLength = DST_SAMPLE_RATE * DST_CHANNEL * DST_CHANNEL_LAYOUT * 16 / 8;

void send_msg_play_req(int msqid, char *pFileName)
{
    tMusicMsg  sendmsg ;

    sendmsg.mtype = AUDIO_PLAY_REQ;
    strncpy(sendmsg.mtext, pFileName, MTEXT_LENGTH);
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_PLAY_REQ: pFileName=%s\n", pFileName);
}

void send_msg_play_cfm(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_PLAY_CFM;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_PLAY_CFM:\n");
}

void send_msg_stop_req(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_STOP_REQ;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_STOP_REQ:\n");
}

void send_msg_stop_cfm(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_STOP_CFM;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_STOP_CFM:\n");
}

void send_msg_file_open_req(int msqid, char *pFileName)
{
    tMusicMsg  sendmsg ;

    sendmsg.mtype = AUDIO_FILE_OPEN_REQ;
    strncpy(sendmsg.mtext, pFileName, MTEXT_LENGTH);
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_FILE_OPEN_REQ: pFileName=%s\n", pFileName);
}

void send_msg_file_open_cfm(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_FILE_OPEN_CFM;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_FILE_OPEN_CFM:\n");
}

void send_msg_file_close_req(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_FILE_CLOSE_REQ;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_FILE_CLOSE_REQ:\n");
}

void send_msg_file_close_cfm(int msqid)
{
    tMusicMsg  sendmsg ;
    sendmsg.mtype = AUDIO_FILE_CLOSE_CFM;
    if (msgsnd(msqid, &sendmsg, MTEXT_LENGTH, 0) < 0) {
        perror("msgsnd() error!!\n");
    }
    AUDIODBG("send AUDIO_FILE_CLOSE_CFM:\n");
}


void stream_close(int sig)
{
    signal(sig, SIG_IGN);
    _gExit = 1;
}

#if RENDER_BY_FFMPEG == 1
int audio_decode_init(tFFContext *pFFCtx, char *pFileName, int *pAudioStreamId)
{
    int i, vRet = 0;
    AVCodec        *pAVCodec;
    AVCodecContext *avCodecContext;
    AVFrame		   *avFrame;
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
        //av_dump_format(pFormatCtxIn, 0, pFileName, 0);

        for (i = 0; i < pFormatCtxIn->nb_streams; i++) {
            if (pFormatCtxIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                *pAudioStreamId = i;
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

        avFrame = av_frame_alloc();

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

        pFFCtx->avFrame = avFrame;
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

    if (avFrame) {
        av_free(avFrame);
    }

    if (avCodecContext) {
        av_free(avCodecContext);
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

    av_free(pFFCtx->avCodecContext);
    av_free(pFFCtx->avFrame);

    if (pFFCtx->pFormatCtxIn) {
        avformat_close_input(&pFFCtx->pFormatCtxIn);
        avformat_free_context(pFFCtx->pFormatCtxIn);
    }

    return RET_SUCCESS;
}

static void *MusicReadThread(void *arg)
{
    int vRet = 0, msgFileId = 0;
    int bytesRead = 0, bytesWrite = 0;
    int writefd = *((int *)arg);
    char pFileName[MTEXT_LENGTH];
    tMusicMsg  recvmsg ;
    unsigned char pBuffer[BUF_SIZE];
    int bFileIsOpen = 0;

    AVPacket avPacketIn;
    int audioStream = -1;
    int dst_linesize, max_dst_nb_samples, dst_nb_samples;

    uint8_t **ppOutputBuffer = NULL;
    tFFContext *pFFCtx = NULL;

    if ((msgFileId = msgget(MUSIC_FILE_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
        printf("Create MUSIC_FILE_MSG_KEY fail\n");
        return NULL;
    }

    pthread_setname_np(pthread_self(), "MusicReadThread");
    pFFCtx = calloc(1, sizeof(tFFContext));
    if (NULL == pFFCtx) {
        perror("malloc fail..");
        return NULL;
    }

    while (!_gExit) {
        if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_OPEN_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_FILE_OPEN_REQ:
                //printf("receive AUDIO_FILE_OPEN_REQ\n");

                strncpy(pFileName, recvmsg.mtext, MTEXT_LENGTH);
                bFileIsOpen = 1;
                memset(pFFCtx, 0, sizeof(tFFContext));
                vRet = audio_decode_init(pFFCtx, pFileName, &audioStream);
                if (RET_FAIL == vRet) {
                    printf("audio_decode_init with %s fail\n", pFileName);
                    return NULL;
                }

                av_init_packet(&avPacketIn);

                max_dst_nb_samples = dst_nb_samples =
                                         av_rescale_rnd(1024, DST_SAMPLE_RATE, pFFCtx->avCodecContext->sample_rate, AV_ROUND_UP);
                vRet = av_samples_alloc_array_and_samples(&ppOutputBuffer, &dst_linesize, DST_CHANNEL, dst_nb_samples, DST_SAMPLE_FMT, 0);
                if (vRet < 0) {
                    printf("av_samples_alloc_array_and_samples() error!!\n");
                    return NULL;
                }

                send_msg_file_open_cfm(msgFileId);
                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }

        if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_CLOSE_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_FILE_CLOSE_REQ:
                //printf("receive AUDIO_FILE_CLOSE_REQ\n");
                bFileIsOpen = 0;

                audio_decode_deinit(pFFCtx);

                if (ppOutputBuffer)
                    av_freep(&ppOutputBuffer[0]);
                av_freep(&ppOutputBuffer);

                send_msg_file_close_cfm(msgFileId);

                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }


        // decode and put pcm data in pipe
        if (bFileIsOpen) {
            bytesWrite = 0;

            if ((vRet = av_read_frame(pFFCtx->pFormatCtxIn, &avPacketIn)) < 0) {
                printf("avcodec_send_packet() error(%d) !! %s \n", AVERROR(vRet), av_err2str(vRet));
                break;
            }

            if (avPacketIn.stream_index == audioStream) {
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
                        bytesRead = av_samples_get_buffer_size(NULL, DST_CHANNEL, vRet, DST_SAMPLE_FMT, 1);
                        while (bytesRead > 0) {
                            if (bytesRead >= PIPE_BUF) {
                                memcpy(pBuffer, &ppOutputBuffer[0][bytesWrite], PIPE_BUF);
                                vRet = write(writefd, pBuffer, PIPE_BUF);
                                if (vRet == -1) {
                                    if (EAGAIN == errno) { // pipe full, sleep to reduce CPU usages
                                        usleep(DELAY_TIME * 1000);
                                        continue;
                                    }
                                    printf("write data fail (%d) %s\n", errno, strerror(errno));
                                    break;
                                } else {
                                    bytesWrite += vRet;
                                    bytesRead -= PIPE_BUF;
                                }
                            } else {
                                memcpy(pBuffer, &ppOutputBuffer[0][bytesWrite], bytesRead);
                                vRet = write(writefd, pBuffer, bytesRead);
                                if (vRet == -1) {
                                    if (EAGAIN == errno) {
                                        usleep(DELAY_TIME * 1000);
                                        continue;
                                    }
                                    printf("write data fail (%d) %s\n", errno, strerror(errno));
                                    break;
                                } else {
                                    bytesWrite += vRet;
                                    bytesRead -= PIPE_BUF;
                                }
                            }
                        }
                    } else {
                        bytesRead = av_samples_get_buffer_size(NULL,
                                                               pFFCtx->avFrame->channels,
                                                               pFFCtx->avFrame->nb_samples,
                                                               pFFCtx->avCodecContext->sample_fmt,
                                                               1);
                        while (bytesRead > 0) {
                            if (bytesRead >= PIPE_BUF) {
                                memcpy(pBuffer, &pFFCtx->avFrame->data[0][bytesWrite], PIPE_BUF);
                                vRet = write(writefd, pBuffer, PIPE_BUF);
                                if (vRet == -1) {
                                    if (EAGAIN == errno) { // pipe full, sleep to reduce CPU usages
                                        usleep(DELAY_TIME * 1000);
                                        continue;
                                    }
                                    printf("write data fail (%d) %s\n", errno, strerror(errno));
                                    break;
                                } else {
                                    bytesWrite += vRet;
                                    bytesRead -= PIPE_BUF;
                                }
                            } else {
                                memcpy(pBuffer, &pFFCtx->avFrame->data[0][bytesWrite], bytesRead);
                                vRet = write(writefd, pBuffer, bytesRead);
                                if (vRet == -1) {
                                    if (EAGAIN == errno) {
                                        usleep(DELAY_TIME * 1000);
                                        continue;
                                    }
                                    printf("write data fail (%d) %s\n", errno, strerror(errno));
                                    break;
                                } else {
                                    bytesWrite += vRet;
                                    bytesRead -= PIPE_BUF;
                                }
                            }
                        }
                    }
                }
            }
        } // end of if(bFileIsOpen)
    }

    if (pFFCtx) {
        free(pFFCtx);
        pFFCtx = NULL;
    }

    printf("%s: _gExit == 1\n", __func__);
    return NULL;
}

#else

static void *MusicReadThread(void *arg)
{
    int vRet = 0, msgFileId = 0;
    int bytesRead = 0, bytesWrite = 0;
    int writefd = *((int *)arg);
    char pFileName[MTEXT_LENGTH];
    tMusicMsg  recvmsg ;
    unsigned char pBuffer[BUF_SIZE];
    int bFileIsOpen = 0;

    FILE *pFileFd;

    if ((msgFileId = msgget(MUSIC_FILE_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
        printf("Create MUSIC_FILE_MSG_KEY fail\n");
        return NULL;
    }

    pthread_setname_np(pthread_self(), "MusicReadThread");

    while (!_gExit) {
        if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_OPEN_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_FILE_OPEN_REQ:
                //printf("receive AUDIO_FILE_OPEN_REQ\n");

                strncpy(pFileName, recvmsg.mtext, MTEXT_LENGTH);
                pFileFd = fopen(pFileName, "rb");
                fseek(pFileFd, 44, SEEK_SET);
                bFileIsOpen = 1;

                send_msg_file_open_cfm(msgFileId);
                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }

        if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_CLOSE_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_FILE_CLOSE_REQ:
                //printf("receive AUDIO_FILE_CLOSE_REQ\n");

                fclose(pFileFd);
                bFileIsOpen = 0;


                send_msg_file_close_cfm(msgFileId);

                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }

        // decode and put pcm data in pipe
        if (bFileIsOpen) {
            bytesWrite = 0;
            bytesRead = fread(pBuffer, 1, 1600, pFileFd);

            while (bytesRead > 0) {
                if (bytesRead >= PIPE_BUF) {
                    vRet = write(writefd, &pBuffer[bytesWrite], PIPE_BUF);
                    if (vRet == -1) {
                        if (EAGAIN == errno) {
                            usleep(DELAY_TIME * 1000);
                            continue;
                        }
                        printf("write data fail (%d) %s\n", errno, strerror(errno));
                        break;
                    } else {
                        bytesWrite += vRet;
                        bytesRead  -= vRet;
                    }
                } else {
                    vRet = write(writefd, &pBuffer[bytesWrite], bytesRead);
                    if (vRet == -1) {
                        if (EAGAIN == errno) {
                            usleep(DELAY_TIME * 1000);
                            continue;
                        }
                        printf("write data fail (%d) %s\n", errno, strerror(errno));
                        break;
                    } else {
                        bytesWrite += vRet;
                        bytesRead  -= vRet;
                    }
                }
            }
        } // end of if(bFileIsOpen)
    }

    printf("%s: _gExit == 1\n", __func__);
    return NULL;
}
#endif


static void *MusicPlayThread(void *arg)
{
    int vRet = 0, msqAudioId = 0, msgFileId = 0;
    int bPlayState = 0;
    int vBufSize = 0;
    int readfd = *((int *)arg);
    struct Cmd vxCmd = {0};
    tMusicMsg recvmsg;

    fd_set set;
    struct timeval timeout;

    unsigned char pBuffer[BUF_SIZE];
    struct pcm *pPcm;

    if ((msqAudioId = msgget(MUSIC_AUDIO_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
        printf("Create MUSIC_AUDIO_MSG_KEY fail\n");
        return NULL;
    }

    if ((msgFileId = msgget(MUSIC_FILE_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
        printf("Create MUSIC_FILE_MSG_KEY fail\n");
        return NULL;
    }

    printf("msqAudioId = %d, msgFileId = %d\n", msqAudioId, msgFileId );

    pthread_setname_np(pthread_self(), "MusicPlayThread");

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

    // TODO: maybe move pcm_open/pcm_close in main loop
    pPcm = pcm_open(vxCmd.card, vxCmd.device, vxCmd.flags, &vxCmd.config);
    if (pPcm == NULL) {
        fprintf(stderr, "failed to allocate memory for pcm\n");
        return NULL;
    } else if (!pcm_is_ready(pPcm)) {
        fprintf(stderr, "failed to open pcm(%p) %u,%u\n", pPcm, vxCmd.card, vxCmd.device);
        return NULL;
    }

    while (!_gExit) {
        if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_PLAY_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_PLAY_REQ:
                //printf("receive AUDIO_PLAY_REQ\n");
                send_msg_file_open_req(msgFileId, recvmsg.mtext);

                do {
                    if ((vRet = msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_OPEN_CFM, IPC_NOWAIT)) > 0) {
                        switch (recvmsg.mtype) {
                        case AUDIO_FILE_OPEN_CFM:
                            //printf("receive AUDIO_FILE_OPEN_CFM\n");
                            break;
                        default:
                            printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                            break;
                        }
                        break;
                    } else {
                        if (EAGAIN == errno) {
                            continue;
                        }
                        //printf("errno is (%d). %s\n", errno, strerror(errno));
                        //break;
                    }
                    if (_gExit) break;
                } while (1);

                bPlayState = 1;
                send_msg_play_cfm(msqAudioId);
                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }

        if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_STOP_REQ, IPC_NOWAIT) > 0) {
            switch (recvmsg.mtype) {
            case AUDIO_STOP_REQ:
                //printf("receive AUDIO_STOP_REQ\n");
                send_msg_file_close_req(msgFileId);

                // drain the pipe
                do {
                    vBufSize = read(readfd, pBuffer, BUF_SIZE);
                } while (vBufSize > 0);

                do {
                    if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, AUDIO_FILE_CLOSE_CFM, IPC_NOWAIT) > 0) {
                        switch (recvmsg.mtype) {
                        case AUDIO_FILE_CLOSE_CFM:
                            //printf("receive AUDIO_FILE_CLOSE_CFM\n");
                            break;
                        default:
                            printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                            break;

                        }
                        break;
                    } else {
                        if (EAGAIN == errno) {
                            continue;
                        }
                        //printf("errno is (%d). %s\n", errno, strerror(errno));
                        //break;
                    }
                    if (_gExit) break;
                } while (1);

                bPlayState = 0;
                send_msg_stop_cfm(msqAudioId);
                break;

            default:
                printf("error message : %lu", recvmsg.mtype);
                break;
            }
        }

        // read pcm data and write to sound device
        if (bPlayState == 1) {
            FD_ZERO(&set);
            FD_SET(readfd, &set);

            timeout.tv_sec = 0;
            timeout.tv_usec = 20 * 1000;
            vRet = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
            if (vRet == 0) {
                // timeout, do nothing
            } else if (vRet < 0) {
                // error occurred
                printf("select error\n");
                break;
            } else if (FD_ISSET(readfd, &set)) {
                // either readfd or pPcm should be blocking I/O
                vBufSize = read(readfd, pBuffer, BUF_SIZE);
                if (vBufSize > 0) {
                    if ((vRet = pcm_writei(pPcm, pBuffer, vBufSize / 4)) < 0) {
                        fprintf(stderr, "error playing sample (%d)\n", vRet);
                        break;
                    }
                }
            }
        }
    }

    if (pPcm != NULL) {
        pcm_close(pPcm);
    }

    printf("%s: _gExit == 1\n", __func__);

    return NULL;
}


int main(int argc, char **argv)
{
    int vRet = RET_SUCCESS, i;
    int msqAudioId = 0, msgFileId = 0;
    int pipefd[2];
    int flags;
    struct sigaction action, old_action;

    tMusicMsg recvmsg;
    pthread_t threadMusicPlay, threadMusicRead;

    if ( argc != 2 ) {
        fprintf( stderr, "Usage: %s SampleAudio.mp3\n", argv[0] );
    }

    /* catch ctrl-c to shutdown cleanly */
    action.sa_handler = stream_close;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_flags |= SA_RESTART;
    sigaction(SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) {
        sigaction(SIGINT, &action, NULL);
    }

    if ( access( argv[1], F_OK ) != -1 ) {

#if 0
        if (pipe2(pipefd, O_DIRECT | O_NONBLOCK) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
#else
        if (pipe2(pipefd, O_DIRECT) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        flags = fcntl(pipefd[0], F_GETFL);
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
#endif
        if ((msqAudioId = msgget(MUSIC_AUDIO_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
            printf("Create MUSIC_AUDIO_MSG_KEY fail\n");
            return RET_FAIL;
        }

        if ((msgFileId = msgget(MUSIC_FILE_MSG_KEY, PERMS | IPC_CREAT)) < 0) {
            printf("Create MUSIC_FILE_MSG_KEY fail\n");
            return RET_FAIL;
        }

        // remove all old command in message queue
        do {
            if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, 0, IPC_NOWAIT) < 0) {
                break;
            }
            printf("audio message queue old commad: %lu\n", recvmsg.mtype);
        } while (1);

        do {
            if (msgrcv(msgFileId, &recvmsg, MTEXT_LENGTH, 0, IPC_NOWAIT) < 0) {
                break;
            }
            printf("file message queue old commad: %lu\n", recvmsg.mtype);
        } while (1);

        vRet = pthread_create(&threadMusicRead, NULL, &MusicReadThread, &pipefd[1]);
        if (vRet != RET_SUCCESS) {
            printf("threadMusicRead error\n");
            return RET_FAIL;
        }

        vRet = pthread_create(&threadMusicPlay, NULL, &MusicPlayThread, &pipefd[0]);
        if (vRet != RET_SUCCESS) {
            printf("threadMusicPlay error\n");
            return RET_FAIL;
        }

        for (i = 0; i < 2; i++) {
            printf("\n\n========\nPlay Count = %d\n", i + 1);
            send_msg_play_req(msqAudioId, argv[1]);

#if 0
            if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_PLAY_CFM, 0) > 0) {
                switch (recvmsg.mtype) {
                case AUDIO_PLAY_CFM:
                    //printf("receive AUDIO_PLAY_CFM\n");
                    break;
                default:
                    printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                    break;
                }
            }
#else
            while (!_gExit) {
                if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_PLAY_CFM, IPC_NOWAIT) > 0) {
                    switch (recvmsg.mtype) {
                    case AUDIO_PLAY_CFM:
                        //printf("receive AUDIO_PLAY_CFM\n");
                        break;
                    default:
                        printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                        break;
                    }
                    break;
                } else {
                    if (EAGAIN == errno) {
                        continue;
                    }
                    //printf("errno is (%d). %s", errno, strerror(errno));
                }
            }
#endif

            printf("sleep(10)\n");
            sleep(10);


            send_msg_stop_req(msqAudioId);
#if 0
            if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_STOP_CFM, 0) > 0) {
                switch (recvmsg.mtype) {
                case AUDIO_STOP_CFM:
                    //printf("receive AUDIO_STOP_CFM\n");
                    break;
                default:
                    printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                    break;
                }
            }
#else
            while (!_gExit) {
                if (msgrcv(msqAudioId, &recvmsg, MTEXT_LENGTH, AUDIO_STOP_CFM, IPC_NOWAIT) > 0) {
                    switch (recvmsg.mtype) {
                    case AUDIO_STOP_CFM:
                        //printf("receive AUDIO_STOP_CFM\n");
                        break;
                    default:
                        printf("%s unexpected message %lu\n", __func__, recvmsg.mtype);
                        break;
                    }
                    break;
                } else {
                    if (EAGAIN == errno) {
                        continue;
                    }
                    //printf("errno is (%d). %s", errno, strerror(errno));
                }
            }
#endif
        }
    } else {
        printf("%s doesn't exist\n", argv[1]);
    }

    _gExit = 1;
    printf("%s: _gExit == 1\n", __func__);

    close(pipefd[0]);
    close(pipefd[1]);

    pthread_join(threadMusicPlay, NULL);
    pthread_join(threadMusicRead, NULL);

    printf("exit main()\n");

    return RET_SUCCESS;
}