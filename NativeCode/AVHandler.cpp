//========= Copyright 2015-2016, HTC Corporation. All rights reserved. ===========

#include "AVHandler.h"
#include "DecoderFFmpeg.h"
#include "Logger.h"

AVHandler::AVHandler() {
	mDecoderState = UNINITIALIZED;
	mSeekTime = 0.0;
	mIDecoder = new DecoderFFmpeg();
}

void AVHandler::init(const char* filePath) {
	if (mIDecoder->init(filePath)) {
		mDecoderState = INITIALIZED;
	} else {
		mDecoderState = INIT_FAIL;
	}
}

AVHandler::DecoderState AVHandler::getDecoderState() {
	return mDecoderState;
}

void AVHandler::stopDecoding() {
	mDecoderState = DECODE_EOF;
	if (mDecodeThread.joinable()) {
		mDecodeThread.join();
	}

	if (mIDecoder != NULL) {
		delete mIDecoder;
		mIDecoder = NULL;
	}
	mDecoderState = UNINITIALIZED;
}

double AVHandler::getVideoFrame(uint8_t** outputY, uint8_t** outputU, uint8_t** outputV) {
	if (!mIDecoder->getVideoInfo().isEnabled || mDecoderState == SEEK) {
		LOG("Video is not available. \n");
		*outputY = *outputU = *outputV = NULL;
		return -1;
	}

	return mIDecoder->getVideoFrame(outputY, outputU, outputV);
}

double AVHandler::getAudioFrame(uint8_t** outputFrame, int& frameSize) {
	if (!mIDecoder->getAudioInfo().isEnabled || mDecoderState == SEEK) {
		LOG("Audio is not available. \n");
		*outputFrame = NULL;
		return -1;
	}
	
	return mIDecoder->getAudioFrame(outputFrame, frameSize);
}

void AVHandler::freeVideoFrame() {
	if (!mIDecoder->getVideoInfo().isEnabled || mDecoderState == SEEK) {
		LOG("Video is not available. \n");
		return;
	}

	mIDecoder->freeVideoFrame();
}

void AVHandler::freeAudioFrame() {
	if (!mIDecoder->getAudioInfo().isEnabled || mDecoderState == SEEK) {
		LOG("Audio is not available. \n");
		return;
	}

	mIDecoder->freeAudioFrame();
}

void AVHandler::startDecoding() {
	if (mDecoderState != INITIALIZED) {
		LOG("Not initialized, decode thread would not start. \n");
	} else {
		mDecodeThread = std::thread([&]() {
			if (!(mIDecoder->getVideoInfo().isEnabled || mIDecoder->getAudioInfo().isEnabled)) {
				LOG("No stream enabled. \n");
				LOG("Decode thread would not start. \n");
				return;
			}

			mDecoderState = DECODING;
			while (mDecoderState != DECODE_EOF) {
				if (mDecoderState == SEEK) {
					mIDecoder->seek(mSeekTime);
					mDecoderState = mLastState;
				}

				mIDecoder->decode();
			}
		});
	}
}

AVHandler::~AVHandler() {
	stopDecoding();
}

void AVHandler::setSeekTime(float sec) {
	if (mDecoderState < INITIALIZED || mDecoderState == SEEK) {
		LOG("Seek unavaiable.");
		return;
	} else {
		mSeekTime = sec;
		mLastState = mDecoderState;
		mDecoderState = SEEK;
	}
}

IDecoder::VideoInfo AVHandler::getVideoInfo() {
	return mIDecoder->getVideoInfo();
}
IDecoder::AudioInfo AVHandler::getAudioInfo() {
	return mIDecoder->getAudioInfo();
}
bool AVHandler::isBufferEmpty() {
	IDecoder::VideoInfo* videoInfo = &(mIDecoder->getVideoInfo());
	IDecoder::AudioInfo* audioInfo = &(mIDecoder->getAudioInfo());
	IDecoder::BufferState EMPTY = IDecoder::BufferState::EMPTY;
	return (videoInfo->isEnabled && videoInfo->bufferState == EMPTY) || (audioInfo->isEnabled && audioInfo->bufferState == EMPTY);
}
bool AVHandler::isBufferFull() {
	IDecoder::VideoInfo* videoInfo = &(mIDecoder->getVideoInfo());
	IDecoder::AudioInfo* audioInfo = &(mIDecoder->getAudioInfo());
	IDecoder::BufferState FULL = IDecoder::BufferState::FULL;
	return (videoInfo->isEnabled && videoInfo->bufferState == FULL) || (audioInfo->isEnabled && audioInfo->bufferState == FULL);
}

int AVHandler::getMetaData(char**& key, char**& value) {
	if (mDecoderState <= UNINITIALIZED) {
		return 0;
	}
	
	return mIDecoder->getMetaData(key, value);
}