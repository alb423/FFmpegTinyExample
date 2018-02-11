#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ffmpeg_decode.h"

extern int decode_video(char *pFileName);
int main(int argc, char **argv)
{
	int ret = RET_SUCCESS;
    if( argc != 2 )
    {
        fprintf( stderr, "Usage: %s SampleVideo_1280x720_1mb.mp4\n", argv[0] );
    }
	
    decode_video(argv[1]);
	if(ret == RET_SUCCESS) {
		printf("ffplay -f rawvideo -pix_fmt yuv420p -video_size 1280x720  ./output.yuv\n");
	}
	else {
		printf("decode %s error\n", argv[1]);
	}
	
    // ffplay -f rawvideo -pixel_format yuv420 -video_size 1280x640  ./output.yuv
    // ffplay -f rawvideo -pix_fmt yuv420p -video_size 1280x720  ./output.yuv
	
    return RET_SUCCESS;    
}