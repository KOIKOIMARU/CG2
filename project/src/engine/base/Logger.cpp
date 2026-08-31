#include "engine/base/Logger.h"
#include <Windows.h>

namespace Logger {

    void Log(const std::string& message) {
        // デバッガ未接続時は表示されないが、ファイルI/Oを伴わず低コストで利用できる。
        OutputDebugStringA(message.c_str());
    }

}
