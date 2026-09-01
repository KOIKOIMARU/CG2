#pragma once
#include <string>

// Windows API境界で使用するUTF-8とUTF-16の相互変換。
namespace StringUtility {
	// UTF-8のstringをUTF-16のwstringへ変換する。
	std::wstring ConvertString(const std::string& str);
	// UTF-16のwstringをUTF-8のstringへ変換する。
	std::string ConvertString(const std::wstring& str);
}
