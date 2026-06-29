#include "App.h"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    App app;
    if (!app.Initialize(instance))
    {
        return 1;
    }

    return app.Run();
}
