#pragma once
#include <string>

// Visual Studioの出力ウィンドウへ診断文字列を送る軽量ロガー。
namespace Logger {
    // 改行は自動付与しないため、必要ならmessage側へ含める。
    void Log(const std::string& message);
}
