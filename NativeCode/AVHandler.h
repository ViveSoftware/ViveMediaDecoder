//========= Copyright 2015-2016, HTC Corporation. All rights reserved. ===========

#pragma once
#include "IDecoder.h"
#include <thread>
 
class AVHandler {
public:
	AVHandler();
	~AVHandler();
	
	enum DecoderState {
		INIT_FAIL = -1, UNINITIALIZED, INITIALIZED, DECODING, SEEK, BUFFERING, DECODE_EOF
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

	IDecoder::VideoInfo getVideoInfo();
	IDecoder::AudioInfo getAudioInfo();
	bool isBufferEmpty();
	bool isBufferFull();

	int getMetaData(char**& key, char**& value);
private:
	DecoderState mDecoderState;
	DecoderState mLastState;
	IDecoder* mIDecoder;
	double mSeekTime;
	
	std::thread mDecodeThread;
};