#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "ffmpeg_decode.h"

extern int decode_video(char *pFileName, enum AVPixelFormat *pPixFmt, int *pWidth, int *pHeight);
int main(int argc, char **argv)
{
    int ret = RET_SUCCESS;
    int width = 0, height = 0;
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    //AV_PIX_FMT_NONE = -1, AV_PIX_FMT_YUV420P,
    if ( argc != 2 ) {
        fprintf( stderr, "Usage: %s SampleVideo.mp4\n", argv[0] );
    }

    if ( access( argv[1], F_OK ) != -1 ) {
        decode_video(argv[1], &pix_fmt, &width, &height);
        if (ret == RET_SUCCESS) {
            printf("RENDER CMD: ffplay -f rawvideo -pix_fmt %s -video_size %dx%d  ./output.yuv\n", av_get_pix_fmt_name(pix_fmt), width, height);
        } else {
            printf("decode %s error\n", argv[1]);
        }
    } else {
        printf("%s doesn't exist\n", argv[1]);
    }
    // ffplay -f rawvideo -pix_fmt yuv420p -video_size 1280x720  ./output.yuv

    return RET_SUCCESS;
}