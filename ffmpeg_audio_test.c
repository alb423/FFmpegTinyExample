#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "ffmpeg_decode.h"

extern int decode_audio(char *pFileName, enum AVSampleFormat *pFmt, int *pRate, int *pChannels);

// 1. To support other audio format, plz reconfigure and rebuild ffmpeg library
// 	  reference https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu
// 2. Below list the file format already tested with a ffmpeg library with default configuration
//     mp3, wav, acc, m4a
int main(int argc, char **argv)
{
    int ret = RET_SUCCESS , n = 1;
    int endian = 0;  // 0: littele endia, 1:big endian
    int sample_rate = 0, channels = 0;
    enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
    if ( argc != 2 ) {
        fprintf( stderr, "Usage: %s SampleAudio.mp3\n", argv[0] );
    }

    // little endian if true
    if (*(char *)&n == 1)
        endian = 0;
    else
        endian = 1;

    if ( access( argv[1], F_OK ) != -1 ) {
        ret = decode_audio(argv[1], &sample_fmt, &sample_rate, &channels);

        if (ret != RET_SUCCESS) {
            printf("decode %s error\n", argv[1]);
            return ret;
        }

        if (endian == 0)
            printf("RENDER CMD: ffplay -f %sle -ar %d -ac %d ./output.pcm\n", \
                   av_get_sample_fmt_name(sample_fmt), sample_rate, channels);
        else
            printf("RENDER CMD: ffplay -f %sbe -ar %d -ac %d ./output.pcm\n", \
                   av_get_sample_fmt_name(sample_fmt), sample_rate, channels);
    } else {
        printf("%s doesn't exist\n", argv[1]);
    }
    // ffplay -f s16le -ar 16000 -ac 1 ./output.pcm

    return RET_SUCCESS;
}

