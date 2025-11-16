#include "engine/base/Logger.h"
#include <debugapi.h>  // または <Windows.h>

namespace Logger {

    void Log(const std::string& message) {
        OutputDebugStringA(message.c_str());
    }

}
