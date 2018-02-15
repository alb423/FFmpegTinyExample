README
===========================
Demo program to use FFmpeg library.

****

|Author|Albert.Liao|
|---|---
|E-mail|alb423@gmail.com

****



## Feature list
1. Decode mp3 file (audio only)
2. Decode mp4 file (video only)

## How to configure ffmpeg library with only mp3 and h264 codec
* wget http://ffmpeg.org/releases/ffmpeg-3.4.1.tar.bz2
* tar xvf ffmpeg-3.4.1.tar.bz2
* cd ffmpeg-3.4.1
* ./configure \
	--arch=x86 \
	--disable-x86asm \
	--target-os=linux \
	--disable-static \
	--enable-shared \
	--disable-debug \
	--disable-avdevice \
	--disable-avfilter \
	--disable-swscale \
	--disable-ffmpeg \
	--disable-ffplay \
	--disable-ffserver \
	--disable-doc \
	--disable-network \
	--disable-muxers \
	--disable-demuxers \
	--enable-rdft \
	--enable-demuxer=mp3 \
	--enable-demuxer=h264 \
	--disable-bsfs \
	--disable-filters \
	--disable-parsers \
	--enable-parser=mpegaudio \
	--disable-protocols \
	--enable-protocol=file \
	--disable-indevs \
	--disable-outdevs \
	--disable-encoders \
	--disable-decoders \
	--enable-decoder=mp3 \
	--enable-decoder=h264
* make clean;make
* make install DESTDIR=~/test/x86_ffmpeg

## How to get test sample
* https://allthingsaudio.wikispaces.com/Sample+Music
* http://techslides.com/sample-webm-ogg-and-mp4-video-files-for-html5
* http://techslides.com/sample-files-for-development
* https://keepvid.com/sites/download-youtube-video.html
* https://linuxconfig.org/ffmpeg-audio-format-conversions

## Usage
* make clean;make
* export LD_LIBRARY_PATH=~/test/x86_ffmpeg/usr/local/lib
* ./audio_test ./SampleAudio_0.4mb.mp3 
* ./vidoe_test ./SampleVideo_1280x720_1mb.mp4

## FFMpeg configuration for embedded arm linux
* ./configure \
	--enable-cross-compile \
	--arch=armv7-a \
	--target-os=linux \
	--cross-prefix=arm-linux-gnueabihf- \
	--enable-neon \
	--extra-cflags='-mfpu=neon -mfloat-abi=hard -mtune=cortex-a7 -std=c99 -fomit-frame-pointer -O3 -fno-math-errno -fno-signed-zeros -fno-tree-vectorize' \
	--disable-static \
	--enable-shared \
	--disable-debug \
	--disable-avdevice \
	--disable-avfilter \
	--disable-swscale \
	--disable-ffmpeg \
	--disable-ffplay \
	--disable-ffserver \
	--disable-doc \
	--disable-network \
	--disable-muxers \
	--disable-demuxers \
	--enable-rdft \
	--enable-demuxer=h264 \
	--enable-demuxer=mp3 \
	--disable-bsfs \
	--disable-filters \
	--disable-parsers \
	--enable-parser=mpegaudio \
	--disable-protocols \
	--enable-protocol=file \
	--disable-indevs \
	--disable-outdevs \
	--disable-encoders \
	--disable-decoders \
	--enable-decoder=h264 \
	--enable-decoder=mp3 \
    --enable-small
	
	
Reference:
* [ffmpeg decode example](https://www.ffmpeg.org/doxygen/2.1/doc_2examples_2decoding_encoding_8c-example.html)
* [ffmpeg new api usage](https://github.com/alb423/FFmpegAudioPlayer/blob/master/FFmpegAudioPlayer/AudioPlayer.m)
* [README.md preview](https://stackedit.io/app)