//========= Copyright 2015-2017, HTC Corporation. All rights reserved. ===========

#include "Unity\IUnityGraphics.h"
#include "MediaDecoder.h"
#include "AVHandler.h"
#include "Logger.h"
#include "DX11TextureObject.h"
#include <stdio.h>
#include <thread>
#include <list>

typedef struct _VideoContext {
	int id = -1;
	char* path = NULL;
	std::thread initThread;
	AVHandler* avhandler = NULL;
	ITextureObject* textureObj = NULL;
	float progressTime = 0.0f;
	float lastUpdateTime = -1.0f;
	bool isContentReady = false;	//	This flag is used to indicate the period that seek over until first data is got.
									//	Usually used for AV sync problem, in pure audio case, it should be discard.
} VideoContext;

ID3D11Device* g_D3D11Device = NULL;
std::list<VideoContext> videoContexts;
typedef std::list<VideoContext>::iterator VideoContextIter;
// --------------------------------------------------------------------------
static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType)
{
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		IUnityGraphicsD3D11* d3d11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
		g_D3D11Device = d3d11->GetDevice();
	}
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	UnityGfxRenderer currentDeviceType = s_DeviceType;
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		s_DeviceType = s_Graphics->GetRenderer();
		currentDeviceType = s_DeviceType;
		break;
	}

	case kUnityGfxDeviceEventShutdown:
		s_DeviceType = kUnityGfxRendererNull;
		break;

	case kUnityGfxDeviceEventBeforeReset:
		break;

	case kUnityGfxDeviceEventAfterReset:
		break;
	};

#if SUPPORT_D3D11
	if (currentDeviceType == kUnityGfxRendererD3D11)
		DoEventGraphicsDeviceD3D11(eventType);
#endif
}

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}
// --------------------------------------------------------------------------

bool getVideoContextIter(int id, VideoContextIter* iter) {
	for (VideoContextIter it = videoContexts.begin(); it != videoContexts.end(); it++) {
		if (it->id == id) {
			*iter = it;
			return true;
		}
	}

	LOG("Decoder does not exist. \n");
	return false;
}

void DoRendering(int id);

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown graphics device type? Do nothing.
	if (s_DeviceType == -1)
		return;

	// Actual functions defined below
	DoRendering(eventID);
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

void DoRendering (int id)
{
	if (s_DeviceType == kUnityGfxRendererD3D11 && g_D3D11Device != NULL)
	{
		ID3D11DeviceContext* ctx = NULL;
		g_D3D11Device->GetImmediateContext (&ctx);

		VideoContextIter iter;
		if (getVideoContextIter(id, &iter)) {
			VideoContext* localVideoContext = &(*iter);
			AVHandler* localAVHandler = localVideoContext->avhandler;

			if (localAVHandler != NULL && localAVHandler->getDecoderState() >= AVHandler::DecoderState::INITIALIZED && localAVHandler->getVideoInfo().isEnabled) {
				if (localVideoContext->textureObj == NULL) {
					unsigned int width = localAVHandler->getVideoInfo().width;
					unsigned int height = localAVHandler->getVideoInfo().height;
					localVideoContext->textureObj = new DX11TextureObject();
					localVideoContext->textureObj->create(g_D3D11Device, width, height);
				}

				double videoDecCurTime = localAVHandler->getVideoInfo().lastTime;
				LOG("videoDecCurTime = %f \n", videoDecCurTime);
				if (videoDecCurTime <= localVideoContext->progressTime) {
					uint8_t* ptrY = NULL;
					uint8_t* ptrU = NULL;
					uint8_t* ptrV = NULL;
					double curFrameTime = localAVHandler->getVideoFrame(&ptrY, &ptrU, &ptrV);
					if (ptrY != NULL && curFrameTime != -1 && localVideoContext->lastUpdateTime != curFrameTime) {
						localVideoContext->textureObj->upload(ptrY, ptrU, ptrV);
						localVideoContext->lastUpdateTime = (float)curFrameTime;
						localVideoContext->isContentReady = true;
					}
					localAVHandler->freeVideoFrame();
				}
			}
		}
		ctx->Release();
	}
}

int nativeCreateDecoderAsync(const char* filePath, int& id) {
	LOG("Query available decoder id. \n");

	int newID = 0;
	VideoContextIter iter;
	while (getVideoContextIter(newID, &iter)) { newID++; }

	VideoContext context;
	context.avhandler = new AVHandler();
	context.id = newID;
	id = context.id;
	context.path = (char*)malloc(sizeof(char) * (strlen(filePath) + 1));
	strcpy_s(context.path, strlen(filePath) + 1, filePath);
	context.isContentReady = false;
	videoContexts.push_back(std::move(context));

	getVideoContextIter(id, &iter);
	iter->initThread = std::thread([iter]() {
		iter->avhandler->init(iter->path);
	});

	return 0;
}

//	Synchronized init. Used for thumbnail currently.
int nativeCreateDecoder(const char* filePath, int& id) {
	LOG("Query available decoder id. \n");

	int newID = 0;
	VideoContextIter iter;
	while (getVideoContextIter(newID, &iter)) { newID++; }

	VideoContext context;
	context.avhandler = new AVHandler();
	context.id = newID;
	id = context.id;
	context.path = NULL;
	context.isContentReady = false;
	context.avhandler->init(filePath);

	videoContexts.push_back(std::move(context));

	return 0;
}

int nativeGetDecoderState(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter) || iter->avhandler == NULL) { return -1; }
		
	return iter->avhandler->getDecoderState();
}

void nativeCreateTexture(int id, void*& tex0, void*& tex1, void*& tex2) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter) || iter->textureObj == NULL) { return; }

	iter->textureObj->getResourcePointers(tex0, tex1, tex2);
}

bool nativeStartDecoding(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter) || iter->avhandler == NULL) { return false; }

	if (iter->initThread.joinable()) {
		iter->initThread.join();
	}

	AVHandler* avhandler = iter->avhandler;
	if (avhandler->getDecoderState() >= AVHandler::DecoderState::INITIALIZED) {
		avhandler->startDecoding();
	}

	if (!avhandler->getVideoInfo().isEnabled) {
		iter->isContentReady = true;
	}

	return true;
}

void nativeDestroyDecoder(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	iter->id = -1;
	if (iter->initThread.joinable()) {
		iter->initThread.join();
	}

	if (iter->avhandler != NULL) {
		delete (iter->avhandler);
		iter->avhandler = NULL;
	}

	if (iter->path != NULL) {
		free(iter->path);
		iter->path = NULL;
	}

	iter->progressTime = 0.0f;
	iter->lastUpdateTime = 0.0f;

	if (iter->textureObj != NULL) {
		delete (iter->textureObj);
		iter->textureObj = NULL;
	}

	iter->isContentReady = false;

	videoContexts.erase(iter);
}

//	Video
bool nativeIsVideoEnabled(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }

	if (iter->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return false;
	}

	bool ret = iter->avhandler->getVideoInfo().isEnabled;
	LOG("nativeIsVideoEnabled: %s \n", ret ? "true" : "false");
	return ret;
}

void nativeGetVideoFormat(int id, int& width, int& height, float& totalTime) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	if (iter->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	IDecoder::VideoInfo* videoInfo = &(iter->avhandler->getVideoInfo());
	width = videoInfo->width;
	height = videoInfo->height;
	totalTime = (float)(videoInfo->totalTime);
}

void nativeSetVideoTime(int id, float currentTime) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	iter->progressTime = currentTime;
}

bool nativeIsAudioEnabled(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }

	if (iter->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return false;
	}

	bool ret = iter->avhandler->getAudioInfo().isEnabled;
	LOG("nativeIsAudioEnabled: %s \n", ret ? "true" : "false");
	return ret;
}

void nativeGetAudioFormat(int id, int& channel, int& frequency, float& totalTime) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	if (iter->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	IDecoder::AudioInfo* audioInfo = &(iter->avhandler->getAudioInfo());
	channel = audioInfo->channels;
	frequency = audioInfo->sampleRate;
	totalTime = (float)(audioInfo->totalTime);
}

float nativeGetAudioData(int id, unsigned char** audioData, int& frameSize) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return -1.0f; }

	return (float) (iter->avhandler->getAudioFrame(audioData, frameSize));
}

void nativeFreeAudioData(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }
	
	iter->avhandler->freeAudioFrame();
}

void nativeSetSeekTime(int id, float sec) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	if (iter->avhandler->getDecoderState() < AVHandler::DecoderState::INITIALIZED) {
		LOG("Decoder is unavailable currently. \n");
		return;
	}

	LOG("nativeSetSeekTime %f. \n", sec);
	iter->avhandler->setSeekTime(sec);
	if (!iter->avhandler->getVideoInfo().isEnabled) {
		iter->isContentReady = true;
	} else {
		iter->isContentReady = false;
	}
}

bool nativeIsSeekOver(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }
	
	return !(iter->avhandler->getDecoderState() == AVHandler::DecoderState::SEEK);
}

bool nativeIsVideoBufferFull(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }

	return iter->avhandler->isVideoBufferFull();
}

bool nativeIsVideoBufferEmpty(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }
	
	return iter->avhandler->isVideoBufferEmpty();
}

/*	This function is for thumbnail extraction.*/
void nativeLoadThumbnail(int id, float time, void* texY, void* texU, void* texV) {
	if (g_D3D11Device == NULL) {
		LOG("g_D3D11Device is null. \n");
		return;
	}

	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	//	1.Initialize variable and texture
	AVHandler* avhandler = iter->avhandler;
	IDecoder::VideoInfo* videoInfo = &(avhandler->getVideoInfo());
	int width = (int) (ceil((float) videoInfo->width / ITextureObject::CPU_ALIGMENT) * ITextureObject::CPU_ALIGMENT);
	int height = videoInfo->height;

	//	2.Get thumbnail data and update texture
	avhandler->setSeekTime(time);
	std::thread thumbnailThread([&]() {
		uint8_t* yptr = NULL, *uptr = NULL, *vptr = NULL;
			
		avhandler->getVideoFrame(&yptr, &uptr, &vptr);
		while (yptr == NULL) {
			avhandler->freeVideoFrame();
			avhandler->getVideoFrame(&yptr, &uptr, &vptr);
		}
			
		ID3D11DeviceContext* ctx = NULL;
		ID3D11Texture2D* d3dtex0 = (ID3D11Texture2D*)texY;
		ID3D11Texture2D* d3dtex1 = (ID3D11Texture2D*)texU;
		ID3D11Texture2D* d3dtex2 = (ID3D11Texture2D*)texV;
			
		g_D3D11Device->GetImmediateContext(&ctx);
		ctx->UpdateSubresource(d3dtex0, 0, NULL, yptr, width, 0);
		ctx->UpdateSubresource(d3dtex1, 0, NULL, uptr, width / 2, 0);
		ctx->UpdateSubresource(d3dtex2, 0, NULL, vptr, width / 2, 0);
		ctx->Release();
	});
	
	if (thumbnailThread.joinable()) {
		thumbnailThread.join();
	}
}

int nativeGetMetaData(const char* filePath, char*** key, char*** value) {
	AVHandler* avhandler = new AVHandler();
	avhandler->init(filePath);

	char** metaKey = NULL;
	char** metaValue = NULL;
	int metaCount = avhandler->getMetaData(metaKey, metaValue);

	*key = (char**)CoTaskMemAlloc(sizeof(char*) * metaCount);
	*value = (char**)CoTaskMemAlloc(sizeof(char*) * metaCount);

	for (int i = 0; i < metaCount; i++) {
		(*key)[i] = (char*)CoTaskMemAlloc(strlen(metaKey[i]) + 1);
		(*value)[i] = (char*)CoTaskMemAlloc(strlen(metaValue[i]) + 1);
		strcpy_s((*key)[i], strlen(metaKey[i]) + 1, metaKey[i]);
		strcpy_s((*value)[i], strlen(metaValue[i]) + 1, metaValue[i]);
	}

	free(metaKey);
	free(metaValue);

	delete avhandler;

	return metaCount;
}

bool nativeIsContentReady(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return false; }

	return iter->isContentReady;
}

void nativeSetVideoEnable(int id, bool isEnable) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	iter->avhandler->setVideoEnable(isEnable);
}

void nativeSetAudioEnable(int id, bool isEnable) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	iter->avhandler->setAudioEnable(isEnable);
}

void nativeSetAudioAllChDataEnable(int id, bool isEnable) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter)) { return; }

	iter->avhandler->setAudioAllChDataEnable(isEnable);
}

bool nativeIsEOF(int id) {
	VideoContextIter iter;
	if (!getVideoContextIter(id, &iter) || iter->avhandler == NULL) { return true; }

	return iter->avhandler->getDecoderState() == AVHandler::DecoderState::DECODE_EOF;
}

//extern "C" EXPORT_API void nativeGetTextureType(void* ptr0) {
//	ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)(ptr0);
//	D3D11_TEXTURE2D_DESC desc;
//	d3dtex->GetDesc(&desc);
//	LOG("Texture format = %d \n", desc.Format);
//}

//int saveByteBuffer(const unsigned char* buff, int fileLength, const char* filePath){
//	FILE *fout = fopen(filePath, "wb+");
//	if (!fout){
//		printf("Can't open output file\n");
//		return 1;
//	}
//	fwrite(buff, sizeof(char)*fileLength, 1, fout);
//	fclose(fout);
//	return NO_ERROR;
//}