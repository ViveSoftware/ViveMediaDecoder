//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

#include "DecoderFFmpeg.h"
#include "Logger.h"
#include <fstream>
#include <string>

DecoderFFmpeg::DecoderFFmpeg() {
	mAVFormatContext = NULL;
	mVideoStream = NULL;
	mAudioStream = NULL;
	mVideoCodec = NULL;
	mAudioCodec = NULL;
	mVideoCodecContext = NULL;
	mAudioCodecContext = NULL;
	av_init_packet(&mPacket);

	mSwrContext = NULL;

	mVideoBuffMax = 64;
	mAudioBuffMax = 128;

	memset(&mVideoInfo, 0, sizeof(VideoInfo));
	memset(&mAudioInfo, 0, sizeof(AudioInfo));
	mIsInitialized = false;
	mIsAudioAllChEnabled = false;
	mUseTCP = false;
	mIsSeekToAny = false;
}

DecoderFFmpeg::~DecoderFFmpeg() {
	destroy();
}

bool DecoderFFmpeg::init(const char* filePath) {
	if (mIsInitialized) {
		LOG("Decoder has been init. \n");
		return true;
	}

	if (filePath == NULL) {
		LOG("File path is NULL. \n");
		return false;
	}

	av_register_all();

	if (mAVFormatContext == NULL) {
		mAVFormatContext = avformat_alloc_context();
	}

	int errorCode = 0;
	errorCode = loadConfig();
	if (errorCode < 0) {
		LOG("config loading error. \n");
		LOG("Use default settings. \n");
		mVideoBuffMax = 64;
		mAudioBuffMax = 128;
		mUseTCP = false;
		mIsSeekToAny = false;
	}

	AVDictionary* opts = NULL;
	if (mUseTCP) {
		av_dict_set(&opts, "rtsp_transport", "tcp", 0);
	}
	
	errorCode = avformat_open_input(&mAVFormatContext, filePath, NULL, &opts);
	av_dict_free(&opts);
	if (errorCode < 0) {
		LOG("avformat_open_input error(%x). \n", errorCode);
		printErrorMsg(errorCode);
		return false;
	}

	errorCode = avformat_find_stream_info(mAVFormatContext, NULL);
	if (errorCode < 0) {
		LOG("avformat_find_stream_info error(%x). \n", errorCode);
		printErrorMsg(errorCode);
		return false;
	}

	double ctxDuration = (double)(mAVFormatContext->duration) / AV_TIME_BASE;

	/* Video initialization */
	int videoStreamIndex = av_find_best_stream(mAVFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (videoStreamIndex < 0) {
		LOG("video stream not found. \n");
		mVideoInfo.isEnabled = false;
	} else {
		mVideoInfo.isEnabled = true;
		mVideoStream = mAVFormatContext->streams[videoStreamIndex];
		mVideoCodecContext = mVideoStream->codec;
		mVideoCodecContext->refcounted_frames = 1;
		mVideoCodec = avcodec_find_decoder(mVideoCodecContext->codec_id);
		
		if (mVideoCodec == NULL) {
			LOG("Video codec not available. \n");
			return false;
		}
		AVDictionary *autoThread = nullptr;
		av_dict_set(&autoThread, "threads", "auto", 0);
		errorCode = avcodec_open2(mVideoCodecContext, mVideoCodec, &autoThread);
		av_dict_free(&autoThread);
		if (errorCode < 0) {
			LOG("Could not open video codec(%x). \n", errorCode);
			printErrorMsg(errorCode);
			return false;
		}

		//	Save the output video format
		//	Duration / time_base = video time (seconds)
		mVideoInfo.width = mVideoCodecContext->width;
		mVideoInfo.height = mVideoCodecContext->height;
		mVideoInfo.totalTime = mVideoStream->duration <= 0 ? ctxDuration : mVideoStream->duration * av_q2d(mVideoStream->time_base);

		mVideoFrames.swap(decltype(mVideoFrames)());
	}

	/* Audio initialization */
	int audioStreamIndex = av_find_best_stream(mAVFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (audioStreamIndex < 0) {
		LOG("audio stream not found. \n");
		mAudioInfo.isEnabled = false;
	} else {
		mAudioInfo.isEnabled = true;
		mAudioStream = mAVFormatContext->streams[audioStreamIndex];
		mAudioCodecContext = mAudioStream->codec;
		mAudioCodec = avcodec_find_decoder(mAudioCodecContext->codec_id);

		if (mAudioCodec == NULL) {
			LOG("Audio codec not available. \n");
			return false;
		}

		errorCode = avcodec_open2(mAudioCodecContext, mAudioCodec, NULL);
		if (errorCode < 0) {
			LOG("Could not open audio codec(%x). \n", errorCode);
			printErrorMsg(errorCode);
			return false;
		}

		errorCode = initSwrContext();
		if (errorCode < 0) {
			LOG("Init SwrContext error.(%x) \n", errorCode);
			printErrorMsg(errorCode);
			return false;
		}

		mAudioFrames.swap(decltype(mAudioFrames)());
	}

	mIsInitialized = true;

	return true;
}

bool DecoderFFmpeg::decode() {
	if (!mIsInitialized) {
		LOG("Not initialized. \n");
		return false;
	}

	if (!isBuffBlocked()) {
		if (av_read_frame(mAVFormatContext, &mPacket) < 0) {
			LOG("End of file.\n");
			return false;
		}

		if (mVideoInfo.isEnabled && mPacket.stream_index == mVideoStream->index) {
			updateVideoFrame();
		} else if (mAudioInfo.isEnabled && mPacket.stream_index == mAudioStream->index) {
			updateAudioFrame();
		}

		av_packet_unref(&mPacket);
		updateBufferState();
	}

	return true;
}

IDecoder::VideoInfo DecoderFFmpeg::getVideoInfo() {
	return mVideoInfo;
}

IDecoder::AudioInfo DecoderFFmpeg::getAudioInfo() {
	return mAudioInfo;
}

void DecoderFFmpeg::setVideoEnable(bool isEnable) {
	if (mVideoStream == NULL) {
		LOG("Video stream not found. \n");
		return;
	}

	mVideoInfo.isEnabled = isEnable;
}

void DecoderFFmpeg::setAudioEnable(bool isEnable) {
	if (mAudioStream == NULL) {
		LOG("Audio stream not found. \n");
		return;
	}

	mAudioInfo.isEnabled = isEnable;
}

void DecoderFFmpeg::setAudioAllChDataEnable(bool isEnable) {
	mIsAudioAllChEnabled = isEnable;
	initSwrContext();
}

int DecoderFFmpeg::initSwrContext() {
	if (mAudioCodecContext == NULL) {
		LOG("Audio context is null. \n");
		return -1;
	}

	int errorCode = 0;
	int64_t inChannelLayout = av_get_default_channel_layout(mAudioCodecContext->channels);
	uint64_t outChannelLayout = mIsAudioAllChEnabled ? inChannelLayout : AV_CH_LAYOUT_STEREO;
	AVSampleFormat inSampleFormat = mAudioCodecContext->sample_fmt;
	AVSampleFormat outSampleFormat = AV_SAMPLE_FMT_FLT;
	int inSampleRate = mAudioCodecContext->sample_rate;
	int outSampleRate = inSampleRate;

	if (mSwrContext != NULL) {
		swr_close(mSwrContext);
		swr_free(&mSwrContext);
		mSwrContext = NULL;
	}

	mSwrContext = swr_alloc_set_opts(NULL,
		outChannelLayout, outSampleFormat, outSampleRate,
		inChannelLayout, inSampleFormat, inSampleRate,
		0, NULL);

	
	if (swr_is_initialized(mSwrContext) == 0) {
		errorCode = swr_init(mSwrContext);
	}

	//	Save the output audio format
	mAudioInfo.channels = av_get_channel_layout_nb_channels(outChannelLayout);
	mAudioInfo.sampleRate = outSampleRate;
	mAudioInfo.totalTime = mAudioStream->duration <= 0 ? (double)(mAVFormatContext->duration) / AV_TIME_BASE : mAudioStream->duration * av_q2d(mAudioStream->time_base);
	
	return errorCode;
}

double DecoderFFmpeg::getVideoFrame(unsigned char** outputY, unsigned char** outputU, unsigned char** outputV) {
	std::lock_guard<std::mutex> lock(mVideoMutex);
	
	if (!mIsInitialized || mVideoFrames.size() == 0) {
		LOG("Video frame not available. \n");
		*outputY = *outputU = *outputV = NULL;
		return -1;
	}

	AVFrame* frame = mVideoFrames.front();
	*outputY = frame->data[0];
	*outputU = frame->data[1];
	*outputV = frame->data[2];

	int64_t timeStamp = av_frame_get_best_effort_timestamp(frame);
	double timeInSec = av_q2d(mVideoStream->time_base) * timeStamp;
	mVideoInfo.lastTime = timeInSec;

	return timeInSec;
}

double DecoderFFmpeg::getAudioFrame(unsigned char** outputFrame, int& frameSize) {
	std::lock_guard<std::mutex> lock(mAudioMutex);
	if (!mIsInitialized || mAudioFrames.size() == 0) {
		LOG("Audio frame not available. \n");
		*outputFrame = NULL;
		return -1;
	}

	AVFrame* frame = mAudioFrames.front();
	*outputFrame = frame->data[0];
	frameSize = frame->nb_samples;
	int64_t timeStamp = av_frame_get_best_effort_timestamp(frame);
	double timeInSec = av_q2d(mAudioStream->time_base) * timeStamp;
	mAudioInfo.lastTime = timeInSec;

	return timeInSec;
}

void DecoderFFmpeg::seek(double time) {
	if (!mIsInitialized) {
		LOG("Not initialized. \n");
		return;
	}

	uint64_t timeStamp = (uint64_t) time * AV_TIME_BASE;

	if (0 > av_seek_frame(mAVFormatContext, -1, timeStamp, mIsSeekToAny ? AVSEEK_FLAG_ANY : AVSEEK_FLAG_BACKWARD)) {
		LOG("Seek time fail.\n");
		return;
	}

	if (mVideoInfo.isEnabled) {
		if (mVideoCodecContext != NULL) {
			avcodec_flush_buffers(mVideoCodecContext);
		}
		flushBuffer(&mVideoFrames, &mVideoMutex);
		mVideoInfo.lastTime = -1;
	}
	
	if (mAudioInfo.isEnabled) {
		if (mAudioCodecContext != NULL) {
			avcodec_flush_buffers(mAudioCodecContext);
		}
		flushBuffer(&mAudioFrames, &mAudioMutex);
		mAudioInfo.lastTime = -1;
	}
}

int DecoderFFmpeg::getMetaData(char**& key, char**& value) {
	if (!mIsInitialized || key != NULL || value != NULL) {
		return 0;
	}

	AVDictionaryEntry *tag = NULL;
	int metaCount = av_dict_count(mAVFormatContext->metadata);

	key = (char**)malloc(sizeof(char*) * metaCount);
	value = (char**)malloc(sizeof(char*) * metaCount);

	for (int i = 0; i < metaCount; i++) {
		tag = av_dict_get(mAVFormatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX);
		key[i] = tag->key;
		value[i] = tag->value;
	}

	return metaCount;
}

void DecoderFFmpeg::destroy() {
	if (mVideoCodecContext != NULL) {
		avcodec_close(mVideoCodecContext);
		mVideoCodecContext = NULL;
	}
	
	if (mAudioCodecContext != NULL) {
		avcodec_close(mAudioCodecContext);
		mAudioCodecContext = NULL;
	}
	
	if (mAVFormatContext != NULL) {
		avformat_close_input(&mAVFormatContext);
		avformat_free_context(mAVFormatContext);
		mAVFormatContext = NULL;
	}
	
	if (mSwrContext != NULL) {
		swr_close(mSwrContext);
		swr_free(&mSwrContext);
		mSwrContext = NULL;
	}
	
	flushBuffer(&mVideoFrames, &mVideoMutex);
	flushBuffer(&mAudioFrames, &mAudioMutex);
	
	mVideoCodec = NULL;
	mAudioCodec = NULL;
	
	mVideoStream = NULL;
	mAudioStream = NULL;
	av_packet_unref(&mPacket);
	
	memset(&mVideoInfo, 0, sizeof(VideoInfo));
	memset(&mAudioInfo, 0, sizeof(AudioInfo));
	
	mIsInitialized = false;
	mIsAudioAllChEnabled = false;
	mVideoBuffMax = 64;
	mAudioBuffMax = 128;
	mUseTCP = false;
	mIsSeekToAny = false;
}

bool DecoderFFmpeg::isBuffBlocked() {
	bool ret = false;
	if (mVideoInfo.isEnabled && mVideoFrames.size() >= mVideoBuffMax) {
		ret = true;
	}

	if (mAudioInfo.isEnabled && mAudioFrames.size() >= mAudioBuffMax) {
		ret = true;
	}

	return ret;
}

void DecoderFFmpeg::updateVideoFrame() {
	int isFrameAvailable = 0;
	AVFrame* frame = av_frame_alloc();
	clock_t start = clock();
	if (avcodec_decode_video2(mVideoCodecContext, frame, &isFrameAvailable, &mPacket) < 0) {
		LOG("Error processing data. \n");
		return;
	}
	LOG("updateVideoFrame = %f\n", (float)(clock() - start) / CLOCKS_PER_SEC);

	if (isFrameAvailable) {
		std::lock_guard<std::mutex> lock(mVideoMutex);
		mVideoFrames.push(frame);
	}
}

void DecoderFFmpeg::updateAudioFrame() {
	int isFrameAvailable = 0;
	AVFrame* frameDecoded = av_frame_alloc();
	if (avcodec_decode_audio4(mAudioCodecContext, frameDecoded, &isFrameAvailable, &mPacket) < 0) {
		LOG("Error processing data. \n");
		return;
	}

	AVFrame* frame = av_frame_alloc();
	frame->sample_rate = frameDecoded->sample_rate;
	frame->channel_layout = av_get_default_channel_layout(mAudioInfo.channels);
	frame->format = AV_SAMPLE_FMT_FLT;	//	For Unity format.
	frame->best_effort_timestamp = frameDecoded->best_effort_timestamp;
	swr_convert_frame(mSwrContext, frame, frameDecoded);

	std::lock_guard<std::mutex> lock(mAudioMutex);
	mAudioFrames.push(frame);
	av_frame_free(&frameDecoded);
}

void DecoderFFmpeg::freeVideoFrame() {
	freeFrontFrame(&mVideoFrames, &mVideoMutex);
}

void DecoderFFmpeg::freeAudioFrame() {
	freeFrontFrame(&mAudioFrames, &mAudioMutex);
}

void DecoderFFmpeg::freeFrontFrame(std::queue<AVFrame*>* frameBuff, std::mutex* mutex) {
	std::lock_guard<std::mutex> lock(*mutex);
	if (!mIsInitialized || frameBuff->size() == 0) {
		LOG("Not initialized or buffer empty. \n");
		return;
	}

	AVFrame* frame = frameBuff->front();
	av_frame_free(&frame);
	frameBuff->pop();
	updateBufferState();
}

//	frameBuff.clear would only clean the pointer rather than whole resources. So we need to clear frameBuff by ourself.
void DecoderFFmpeg::flushBuffer(std::queue<AVFrame*>* frameBuff, std::mutex* mutex) {
	std::lock_guard<std::mutex> lock(*mutex);
	while (!frameBuff->empty()) {
		av_frame_free(&(frameBuff->front()));
		frameBuff->pop();
	}
}

//	Record buffer state either FULL or EMPTY. It would be considered by ViveMediaDecoder.cs for buffering judgement.
void DecoderFFmpeg::updateBufferState() {
	if (mVideoInfo.isEnabled) {
		if (mVideoFrames.size() >= mVideoBuffMax) {
			mVideoInfo.bufferState = BufferState::FULL;
		} else if(mVideoFrames.size() == 0) {
			mVideoInfo.bufferState = BufferState::EMPTY;
		} else {
			mVideoInfo.bufferState = BufferState::NORMAL;
		}
	}

	if (mAudioInfo.isEnabled) {
		if (mAudioFrames.size() >= mAudioBuffMax) {
			mAudioInfo.bufferState = BufferState::FULL;
		} else if (mAudioFrames.size() == 0) {
			mAudioInfo.bufferState = BufferState::EMPTY;
		} else {
			mAudioInfo.bufferState = BufferState::NORMAL;
		}
	}
}

int DecoderFFmpeg::loadConfig() {
	std::ifstream configFile("config", std::ifstream::in);
	if (!configFile) {
		LOG("config does not exist.\n");
		return -1;
	}

	enum CONFIG { NONE, USE_TCP, BUFF_MIN, BUFF_MAX };
	int buffVideoMax = 0, buffAudioMax = 0, tcp = 0, seekAny = 0;
	std::string line;
	while (configFile >> line) {
		std::string token = line.substr(0, line.find("="));
		CONFIG config = NONE;
		std::string value = line.substr(line.find("=") + 1);
		try {
			if (token == "USE_TCP") { tcp = stoi(value); }
			else if (token == "BUFF_VIDEO_MAX") { buffVideoMax = stoi(value); }
			else if (token == "BUFF_AUDIO_MAX") { buffAudioMax = stoi(value); }
			else if (token == "SEEK_ANY") { seekAny = stoi(value); }
		
		} catch (...) {
			return -1;
		}
	}

	mUseTCP = tcp != 0;
	mVideoBuffMax = buffVideoMax;
	mAudioBuffMax = buffAudioMax;
	mIsSeekToAny = seekAny != 0;
	LOG("config loading success.\n");
	LOG("USE_TCP=%s\n", mUseTCP ? "true" : "false");
	LOG("BUFF_VIDEO_MAX=%d\n", mVideoBuffMax);
	LOG("BUFF_AUDIO_MAX=%d\n", mAudioBuffMax);
	LOG("SEEK_ANY=%s\n", mIsSeekToAny ? "true" : "false");

	return 0;
}

void DecoderFFmpeg::printErrorMsg(int errorCode) {
	char msg[500];
	av_strerror(errorCode, msg, sizeof(msg));
	LOG("Error massage: %s \n", msg);
}