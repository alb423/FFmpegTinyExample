#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

extern int decode_audio(char *pFileName);


// Below list the file format already tested
// mp3, wav, acc, 
int main(int argc, char **argv)
{
    int ret = RET_SUCCESS;
    if ( argc != 2 ) {
        fprintf( stderr, "Usage: %s SampleAudio.mp3\n", argv[0] );
    }
    ret = decode_audio(argv[1]);
    if (ret == RET_SUCCESS) {
        printf("RENDER CMD: ffplay -f s16le -ar 16000 -ac 1 ./output.pcm\n");
    } else {
        printf("decode %s error\n", argv[1]);
    }

    return RET_SUCCESS;
}

