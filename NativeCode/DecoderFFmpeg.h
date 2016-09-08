//========= Copyright 2015-2016, HTC Corporation. All rights reserved. ===========

#pragma once
#include "IDecoder.h"
#include <list>

extern "C" {
#include <libavformat\avformat.h>
#include <libswresample\swresample.h>
}

class DecoderFFmpeg : public virtual IDecoder
{
public:
	DecoderFFmpeg();
	~DecoderFFmpeg();

	bool init(const char* filePath);
	bool decode();
	void seek(double time);
	void destroy();
	
	VideoInfo getVideoInfo();
	AudioInfo getAudioInfo();
	double	getVideoFrame(unsigned char** outputY, unsigned char** outputU, unsigned char** outputV);
	double	getAudioFrame(unsigned char** outputFrame, int& frameSize);
	void freeVideoFrame();
	void freeAudioFrame();

	int getMetaData(char**& key, char**& value);
	
private:
	bool isInitialized;

	AVFormatContext* mAVFormatContext;
	AVStream*		mVideoStream;
	AVStream*		mAudioStream;
	AVCodec*		mVideoCodec;
	AVCodec*		mAudioCodec;
	AVCodecContext*	mVideoCodecContext;
	AVCodecContext*	mAudioCodecContext;

	AVPacket	mPacket;
	std::list<AVFrame*> mVideoFrames;
	std::list<AVFrame*> mAudioFrames;
	const unsigned int BUFF_MAX = 128;
	const unsigned int BUFF_MIN = 64;

	SwrContext*	mSwrContext;

	VideoInfo	mVideoInfo;
	AudioInfo	mAudioInfo;
	void updateBufferState();

	int mFrameBufferNum;
	
	bool isBuffBlocked();
	void updateVideoFrame();
	void updateAudioFrame();
	void freeFrontFrame(std::list<AVFrame*>* frameBuff);
	void flushBuffer(std::list<AVFrame*>* frameBuff);

	void printErrorMsg(int errorCode);
};