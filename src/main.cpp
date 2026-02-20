#include "Core/Engine.h"
#include <windows.h>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    RRE::EngineInitParams params;
    params.platformHandle = hInstance;
    params.showCommand = nCmdShow;

    RRE::Engine engine;

    if (!engine.Initialize(params))
    {
        return -1;
    }

    engine.Run();
    engine.Shutdown();

    return 0;
}
