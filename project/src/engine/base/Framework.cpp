#include "engine/base/Framework.h"

void Framework::Run() {
    Initialize();

    while (!isEndRequest_) {
        Update(); // ここでProcessMessageを見てbreakする
        if (isEndRequest_) { break; }
        Draw();
    }

    Finalize();
}
