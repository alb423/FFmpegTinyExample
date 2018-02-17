HOME = /home/albert

# unmark below line to enable render by tinyalsa
#RENDER_BY_TINYALSA = 1

CROSS_COMPILE = 
CC = $(CROSS_COMPILE)gcc

ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
CFLAGS = -I$(HOME)/test/arm_ffmpeg/usr/local/include/ -L$(HOME)/test/arm_ffmpeg/usr/local/lib
else
CFLAGS = -I$(HOME)/test/x86_ffmpeg/usr/local/include/ -L$(HOME)/test/x86_ffmpeg/usr/local/lib
endif

CFLAGS += -Wall
CFLAGS += -std=c99
CFLAGS += -g

LDFLAGS += -lavcodec -lavformat -lavutil -lswresample -lm -lpthread

ifeq ($(RENDER_BY_TINYALSA), 1)
CFLAGS  += -DRENDER_BY_TINYALSA=1
LDFLAGS += -ltinyalsa
endif



all: audio_test video_test

audio_test: ffmpeg_audio_decode.c ffmpeg_audio_test.c
	$(CC) $(CFLAGS) ffmpeg_audio_decode.c ffmpeg_audio_test.c $(LDFLAGS) -o audio_test 

video_test: ffmpeg_video_decode.c ffmpeg_video_test.c
	$(CC) $(CFLAGS) ffmpeg_video_decode.c ffmpeg_video_test.c $(LDFLAGS) -o video_test 
	
clean:
	rm -rf video_test audio_test
	rm -rf output.pcm output.yuv
