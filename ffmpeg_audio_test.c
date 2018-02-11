#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

extern int decode_audio(char *pFileName);

int main(int argc, char **argv)
{
	int ret = RET_SUCCESS;
    if( argc != 2 )
    {
        fprintf( stderr, "Usage: %s SampleAudio_0.4mb.mp3\n", argv[0] );
    }
    ret = decode_audio(argv[1]);
	if(ret == RET_SUCCESS) {
		printf("ffplay -f s16le -ar 44100 -ac 2 ./output.pcm\n");
	}
	else {
		printf("decode %s error\n", argv[1]);
	}
	
    return RET_SUCCESS;    
}

