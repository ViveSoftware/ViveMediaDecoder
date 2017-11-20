//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

#pragma once

class IDecoder
{
public:
	virtual ~IDecoder() {}

	enum BufferState {EMPTY, NORMAL, FULL};

	struct VideoInfo {
		bool isEnabled;
		int width;
		int height;
		double lastTime;
		double totalTime;
		BufferState bufferState;
	};

	struct AudioInfo {
		bool isEnabled;
		unsigned int channels;
		unsigned int sampleRate;
		double lastTime;
		double totalTime;
		BufferState bufferState;
	};
	
	virtual bool init(const char* filePath) = 0;
	virtual bool decode() = 0;
	virtual void seek(double time) = 0;
	virtual void destroy() = 0;

	virtual VideoInfo getVideoInfo() = 0;
	virtual AudioInfo getAudioInfo() = 0;
	virtual void setVideoEnable(bool isEnable) = 0;
	virtual void setAudioEnable(bool isEnable) = 0;
	virtual void setAudioAllChDataEnable(bool isEnable) = 0;
	virtual double getVideoFrame(unsigned char** outputY, unsigned char** outputU, unsigned char** outputV) = 0;
	virtual double getAudioFrame(unsigned char** outputFrame, int& frameSize) = 0;
	virtual void freeVideoFrame() = 0;
	virtual void freeAudioFrame() = 0;

	virtual int getMetaData(char**& key, char**& value) = 0;
};