#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

int audio_decode_init(tFFContext *pFFCtx)
{
    AVFrame       *pAVFrame;
    AVCodec         *pAVCodec;
    AVCodecContext *avCodecContext;

    avcodec_register_all();
    av_register_all();

    pAVCodec = avcodec_find_decoder(pFFCtx->codec_id);

    if (pAVCodec == NULL) {
        printf("Codec not found\n");
        return RET_FAIL;
    }

    avCodecContext = avcodec_alloc_context3(pAVCodec);

    pAVFrame = av_frame_alloc();
    if(pFFCtx->extradata_size >0) {
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


int decode_audio(char *pFileName)
{
    int i, vRet=0, bytes_num=0;
    FILE *pFileOut;
    size_t totalBytesRead=0, totalBytesWrite=0;

    // For file input
	SwrContext *swr;	
    AVFormatContext *pFormatCtxIn;
    AVPacket avPacketIn;
    int audioStream;
	
	int dst_sample_fmt = AV_SAMPLE_FMT_S16;
	int dst_linesize, dst_nb_channels = 2, dst_nb_samples = 10240;  // should large then pFFCtx->avFrame->nb_samples
	
	uint8_t **ppOutputBuffer = NULL;

    // For file output
    tFFContext *pFFCtx=NULL;

	if(!pFileName)
    {
        perror("malloc fail..");
        return RET_FAIL;
    }
	
    pFFCtx = calloc(1, sizeof(tFFContext));
    if(NULL == pFFCtx)
    {
        perror("malloc fail..");
        return RET_FAIL;
    }

    /* Prepare input file, user should replace this section to network buffers */
    avcodec_register_all();
    av_register_all();
    av_init_packet(&avPacketIn);

    pFormatCtxIn = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtxIn, pFileName, NULL, NULL) != 0) {
        perror("fopen failed in main");
        goto error1;
    }    
    avformat_find_stream_info(pFormatCtxIn, NULL);
    av_dump_format(pFormatCtxIn, 0, pFileName, 0);

    for(i=0;i<pFormatCtxIn->nb_streams;i++){
        if(pFormatCtxIn->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            audioStream=i; 
            printf("audioStream=%d\n", audioStream);
            break;
        }
    }

    pFFCtx->codec_id = AV_CODEC_ID_MP3;
    /* End of input prepare */


    audio_decode_init(pFFCtx);

    printf("pFFCtx->avCodecContext->sample_fmt = %s\n\n", av_get_sample_fmt_name(pFFCtx->avCodecContext->sample_fmt));
	
	if(pFFCtx->avCodecContext->sample_fmt != dst_sample_fmt)
	{
		// Set up SWR context once you've got codec information
		swr = swr_alloc();
		av_opt_set_int(swr, "in_channel_layout",  pFormatCtxIn->streams[i]->codecpar->channel_layout, 0);
		av_opt_set_int(swr, "out_channel_layout", pFormatCtxIn->streams[i]->codecpar->channel_layout,  0);
		av_opt_set_int(swr, "in_sample_rate",     pFormatCtxIn->streams[i]->codecpar->sample_rate, 0);
		av_opt_set_int(swr, "out_sample_rate",    pFormatCtxIn->streams[i]->codecpar->sample_rate, 0);
		av_opt_set_sample_fmt(swr, "in_sample_fmt",  AV_SAMPLE_FMT_S16P, 0);
		av_opt_set_sample_fmt(swr, "out_sample_fmt", dst_sample_fmt,  0);
		swr_init(swr);
	}

    /* Prepare output file */
    pFileOut = fopen("./output.pcm", "wb");
    if(NULL == pFileOut)
    {
        perror("fopen failed in main");
        vRet = RET_FAIL;
        goto error2;
    }

    /* Decode data */

    while(av_read_frame(pFormatCtxIn,&avPacketIn)>=0)    
    {
        if(avPacketIn.stream_index==audioStream) 
        {
            totalBytesRead += avPacketIn.size;
            //printf("avPacketIn.size=%d\n", avPacketIn.size);

			avcodec_send_packet(pFFCtx->avCodecContext, &avPacketIn);
			do {
				vRet = avcodec_receive_frame(pFFCtx->avCodecContext, pFFCtx->avFrame);
			} while(vRet==EAGAIN);
			
			if(vRet < 0) {
				/* if error, skip frame */
				break;
			}			
			else {
				
				if(!ppOutputBuffer) {
					vRet = av_samples_alloc_array_and_samples(&ppOutputBuffer, &dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 0);
					if(vRet < 0)
					{
						printf("av_samples_alloc_array_and_samples() error!!\n");
						break;
					}					 
				}
				/*
					AV_SAMPLE_FMT_S16
						c1 c1 c2 c2 c1 c1 c2 c2...
					AV_SAMPLE_FMT_S16P
						c1 c1 c1 c1 .... c2 c2 c2 c2 ..
				*/
				if(pFFCtx->avCodecContext->sample_fmt != dst_sample_fmt) {				
					vRet = swr_convert(swr,
								ppOutputBuffer, 
								pFFCtx->avFrame->nb_samples,
								(const uint8_t **)pFFCtx->avFrame->data, 
								pFFCtx->avFrame->nb_samples);   
					
					bytes_num = av_samples_get_buffer_size(NULL, dst_nb_channels, vRet, dst_sample_fmt, 1);
					vRet = fwrite(ppOutputBuffer[0], 1, bytes_num, pFileOut);
					totalBytesWrite += vRet;
					if(vRet < bytes_num)
					{
						printf("write file error %d!=%d\n", bytes_num, vRet);
						break;
					}					
				}
				else {
					bytes_num = av_samples_get_buffer_size(NULL, 
											pFFCtx->avFrame->channels, 
											pFFCtx->avFrame->nb_samples, 
											pFFCtx->avCodecContext->sample_fmt, 
											1);
					vRet = fwrite(pFFCtx->avFrame->data[0], 1, bytes_num, pFileOut);
					if(vRet < bytes_num)
					{
						printf("write file error %d!=%d\n", bytes_num, vRet);
						break;
					}							
				}
            }
			pFFCtx->frameCount++;
        }
		else {
			printf("unexpected data\n");
		}
    }
	
    printf("totalBytesRead=%zu totalBytesWrite=%zu frameCount=%d\n", totalBytesRead, totalBytesWrite, pFFCtx->frameCount);

    audio_decode_deinit(pFFCtx);

    /*Free and close everything*/    
    fclose(pFileOut);     

error2:
    avformat_close_input(&pFormatCtxIn);
error1:
    free(pFFCtx);
    return RET_SUCCESS;    
}