//========= Copyright 2015-2018, HTC Corporation. All rights reserved. ===========

#pragma once
#include "ITextureObject.h"
#include <d3d11.h>

class DX11TextureObject : public virtual ITextureObject
{
public:
	DX11TextureObject();
	~DX11TextureObject();

	void create(void* handler, unsigned int width, unsigned int height);
	void getResourcePointers(void*& ptry, void*& ptru, void*& ptrv);
	void upload(unsigned char* ych, unsigned char* uch, unsigned char* vch);
	void destroy();

private:
	ID3D11Device* mD3D11Device;
	
	unsigned int mWidthY;
	unsigned int mHeightY;
	unsigned int mLengthY;

	unsigned int mWidthUV;
	unsigned int mHeightUV;
	unsigned int mLengthUV;

	ID3D11Texture2D* mTextures[TEXTURE_NUM];
	ID3D11ShaderResourceView* mShaderResourceView[TEXTURE_NUM];
};