//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

#pragma once
#include "IDecoder.h"
#include <thread>
#include <mutex>
#include <memory>
 
class AVHandler {
public:
	AVHandler();
	~AVHandler();
	
	enum DecoderState {
		INIT_FAIL = -1, UNINITIALIZED, INITIALIZED, DECODING, SEEK, BUFFERING, DECODE_EOF, STOP
	};
	DecoderState getDecoderState();

	void init(const char* filePath);
	void startDecoding();
	void stopDecoding();
	void setSeekTime(float sec);
	
	double getVideoFrame(uint8_t** outputY, uint8_t** outputU, uint8_t** outputV);
	double getAudioFrame(uint8_t** outputFrame, int& frameSize);
	void freeVideoFrame();
	void freeAudioFrame();
	void setVideoEnable(bool isEnable);
	void setAudioEnable(bool isEnable);
	void setAudioAllChDataEnable(bool isEnable);

	IDecoder::VideoInfo getVideoInfo();
	IDecoder::AudioInfo getAudioInfo();
	bool isVideoBufferEmpty();
	bool isVideoBufferFull();

	int getMetaData(char**& key, char**& value);

private:
	DecoderState mDecoderState;
	std::unique_ptr<IDecoder> mIDecoder;
	double mSeekTime;
	
	std::thread mDecodeThread;
};