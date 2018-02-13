// TODO: check resource leakage and error control

// Reference
// https://fossies.org/linux/ffmpeg/doc/examples/decoding_encoding.c
// https://github.com/alb423/FFmpegAudioPlayer/blob/master/FFmpegAudioPlayer/AudioPlayer.m
// https://github.com/felipec/libomxil-bellagio/blob/master/src/components/ffmpeg/omx_videodec_component.c

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#define RET_SUCCESS		0
#define RET_FAIL   		-1


extern void avcodec_register_all(void);
extern int avcodec_open2 (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
extern int av_image_get_buffer_size(enum AVPixelFormat pix_fmt, int width, int height, int align);
extern AVCodecContext * avcodec_alloc_context3(const AVCodec *codec);
extern AVFrame * av_frame_alloc(void);
extern const char *    av_get_pix_fmt_name (enum AVPixelFormat pix_fmt);
extern const char *    av_color_space_name (enum AVColorSpace space);
extern int av_pix_fmt_count_planes (   enum AVPixelFormat  pix_fmt);

#define __TRACE__ printf("%s:%d\n", __func__, __LINE__);

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
