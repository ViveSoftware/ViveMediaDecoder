//========= Copyright 2015-2017, HTC Corporation. All rights reserved. ===========

#include "DX11TextureObject.h"
#include "Logger.h"
#include <thread>

DX11TextureObject::DX11TextureObject() {
	mD3D11Device = NULL;
	mWidthY = mHeightY = mLengthY = 0;
	mWidthUV = mHeightUV = mLengthUV = 0;
	
	for (int i = 0; i < TEXTURE_NUM; i++) {
		mTextures[i] = NULL;
		mShaderResourceView[i] = NULL;
	}
}

DX11TextureObject::~DX11TextureObject() {
	destroy();
}

void DX11TextureObject::getResourcePointers(void*& ptry, void*& ptru, void*& ptrv) {
	if (mD3D11Device == NULL) {
		return;
	}

	ptry = mShaderResourceView[0];
	ptru = mShaderResourceView[1];
	ptrv = mShaderResourceView[2];
}

void DX11TextureObject::create(void* handler, unsigned int width, unsigned int height) {
	if (handler == NULL) {
		return;
	}

	mD3D11Device = (ID3D11Device*) handler;
	mWidthY = (unsigned int)(ceil((float) width / CPU_ALIGMENT) * CPU_ALIGMENT);
	mHeightY = height;
	mLengthY = mWidthY * mHeightY;

	mWidthUV = mWidthY / 2;
	mHeightUV = mHeightY / 2;
	mLengthUV = mWidthUV * mHeightUV;

	//	For YUV420
	//	Y channel
	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DYNAMIC;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texDesc.MiscFlags = 0;

	HRESULT result = mD3D11Device->CreateTexture2D(&texDesc, NULL, (ID3D11Texture2D**)(&(mTextures[0])));
	if (FAILED(result)) {
		LOG("Create texture Y fail. Error code: %x\n", result);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	shaderResourceViewDesc.Format = DXGI_FORMAT_A8_UNORM;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;

	result = mD3D11Device->CreateShaderResourceView((ID3D11Texture2D*)(mTextures[0]), &shaderResourceViewDesc, &(mShaderResourceView[0]));
	if (FAILED(result)) {
		LOG("Create shader resource view Y fail. Error code: %x\n", result);
	}

	//	UV channel
	texDesc.Width = width / 2;
	texDesc.Height = height / 2;
	result = mD3D11Device->CreateTexture2D(&texDesc, NULL, (ID3D11Texture2D**)(&(mTextures[1])));
	if (FAILED(result)) {
		LOG("Create texture U fail. Error code: %x\n", result);
	}

	result = mD3D11Device->CreateShaderResourceView((ID3D11Texture2D*)(mTextures[1]), &shaderResourceViewDesc, &(mShaderResourceView[1]));
	if (FAILED(result)) {
		LOG("Create shader resource view U fail. Error code: %x\n", result);
	}

	result = mD3D11Device->CreateTexture2D(&texDesc, NULL, (ID3D11Texture2D**)(&(mTextures[2])));
	if (FAILED(result)) {
		LOG("Create texture V fail. Error code: %x\n", result);
	}

	result = mD3D11Device->CreateShaderResourceView((ID3D11Texture2D*)(mTextures[2]), &shaderResourceViewDesc, &(mShaderResourceView[2]));
	if (FAILED(result)) {
		LOG("Create shader resource view V fail. %x\n", result);
	}
}

void DX11TextureObject::upload(unsigned char* ych, unsigned char* uch, unsigned char* vch) {
	if (mD3D11Device == NULL) {
		return;
	}

	ID3D11DeviceContext* ctx = NULL;
	mD3D11Device->GetImmediateContext(&ctx);
	
	D3D11_MAPPED_SUBRESOURCE mappedResource[TEXTURE_NUM];
	for (int i = 0; i < TEXTURE_NUM; i++) {
		ZeroMemory(&(mappedResource[i]), sizeof(D3D11_MAPPED_SUBRESOURCE));
		ctx->Map(mTextures[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &(mappedResource[i]));
	}

	//	Consider padding.
	UINT rowPitchY = mappedResource[0].RowPitch;
	UINT rowPitchUV = mappedResource[1].RowPitch;

	uint8_t* ptrMappedY = (uint8_t*)(mappedResource[0].pData);
	uint8_t* ptrMappedU = (uint8_t*)(mappedResource[1].pData);
	uint8_t* ptrMappedV = (uint8_t*)(mappedResource[2].pData);

	//	Two thread memory copy
	std::thread YThread = std::thread([&]() {
		//	Map region has its own row pitch which may different to texture width.
		if (mWidthY == rowPitchY) {
			memcpy(ptrMappedY, ych, mLengthY);
		}
		else {
			//	Handle rowpitch of mapped memory.
			uint8_t* end = ych + mLengthY;
			while (ych != end) {
				memcpy(ptrMappedY, ych, mWidthY);
				ych += mWidthY;
				ptrMappedY += rowPitchY;
			}
		}
	});

	std::thread UVThread = std::thread([&]() {
		if (mWidthUV == rowPitchUV) {
			memcpy(ptrMappedU, uch, mLengthUV);
			memcpy(ptrMappedV, vch, mLengthUV);
		}
		else {
			//	Handle rowpitch of mapped memory.
			//	YUV420, length U == length V
			uint8_t* endU = uch + mLengthUV;
			while (uch != endU) {
				memcpy(ptrMappedU, uch, mWidthUV);
				memcpy(ptrMappedV, vch, mWidthUV);
				uch += mWidthUV;
				vch += mWidthUV;
				ptrMappedU += rowPitchUV;
				ptrMappedV += rowPitchUV;
			}
		}
	});

	if (YThread.joinable()) {
		YThread.join();
	}
	if (UVThread.joinable()) {
		UVThread.join();
	}

	for (int i = 0; i < TEXTURE_NUM; i++) {
		ctx->Unmap(mTextures[i], 0);
	}
	ctx->Release();
}

void DX11TextureObject::destroy() {
	mD3D11Device = NULL;
	mWidthY = mHeightY = mLengthY = 0;
	mWidthUV = mHeightUV = mLengthUV = 0;

	for (int i = 0; i < TEXTURE_NUM; i++) {
		if (mTextures[i] != NULL) {
			mTextures[i]->Release();
			mTextures[i] = NULL;
		}

		if (mShaderResourceView[i] != NULL) {
			mShaderResourceView[i]->Release();
			mShaderResourceView[i] = NULL;
		}
	}
}