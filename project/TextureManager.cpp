#include "TextureManager.h"

TextureManager* TextureManager::instance_ = nullptr;

void TextureManager::Initialize()
{
	// SRVの数と同数
	textureDatas_.resize(DirectXCommon::kMaxSRVCount);
}

TextureManager* TextureManager::GetInstance()
{
	if (instance_ == nullptr) {
		instance_ = new TextureManager();
	}
	return instance_;
}

void TextureManager::Finalize()
{
	delete instance_;
	instance_ = nullptr;
}
