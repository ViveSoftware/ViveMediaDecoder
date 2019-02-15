//========= Copyright 2015-2019, HTC Corporation. All rights reserved. ===========

#pragma once
#include "IDecoder.h"
#include <queue>
#include <mutex>

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
	void setVideoEnable(bool isEnable);
	void setAudioEnable(bool isEnable);
	void setAudioAllChDataEnable(bool isEnable);
	double	getVideoFrame(unsigned char** outputY, unsigned char** outputU, unsigned char** outputV);
	double	getAudioFrame(unsigned char** outputFrame, int& frameSize);
	void freeVideoFrame();
	void freeAudioFrame();

	int getMetaData(char**& key, char**& value);
	
private:
	bool mIsInitialized;
	bool mIsAudioAllChEnabled;
	bool mUseTCP;				//	For RTSP stream.

	AVFormatContext* mAVFormatContext;
	AVStream*		mVideoStream;
	AVStream*		mAudioStream;
	AVCodec*		mVideoCodec;
	AVCodec*		mAudioCodec;
	AVCodecContext*	mVideoCodecContext;
	AVCodecContext*	mAudioCodecContext;

	AVPacket	mPacket;
	std::queue<AVFrame*> mVideoFrames;
	std::queue<AVFrame*> mAudioFrames;
	unsigned int mVideoBuffMax;
	unsigned int mAudioBuffMax;

	SwrContext*	mSwrContext;
	int initSwrContext();

	VideoInfo	mVideoInfo;
	AudioInfo	mAudioInfo;
	void updateBufferState();

	int mFrameBufferNum;
	
	bool isBuffBlocked();
	void updateVideoFrame();
	void updateAudioFrame();
	void freeFrontFrame(std::queue<AVFrame*>* frameBuff, std::mutex* mutex);
	void flushBuffer(std::queue<AVFrame*>* frameBuff, std::mutex* mutex);
	std::mutex mVideoMutex;
	std::mutex mAudioMutex;
	
	bool mIsSeekToAny;

	int loadConfig();
	void printErrorMsg(int errorCode);
};