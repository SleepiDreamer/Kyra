#include <windows.h>
#include "Application.h"
#include "Log.h"

#include <windows.h>
#include <iostream>
#include <vector>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 619; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

int main(int argc, char* argv[])
{
    bool enableDebug = false;
    std::vector<std::string> inputPaths;
    std::string worldsRoot;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--debuglayer") == 0) 
        {
            enableDebug = true;
        }
        else if (strcmp(argv[i], "-i") == 0) 
        {
            if (i + 1 >= argc) 
            {
				Log::Error("-i requires a path argument");
                return 1;
            }
            inputPaths.emplace_back(argv[++i]);
        }
        else if (strcmp(argv[i], "--worlds") == 0)
        {
            // Folder holding one Minecraft save per model, each named after the
            // model file. Optional: without it the folder is searched for next
            // to the executable and the project.
            if (i + 1 >= argc)
            {
				Log::Error("--worlds requires a path argument");
                return 1;
            }
            worldsRoot = argv[++i];
        }
        else
        {
			Log::Warning("Unknown argument: {}", std::string(argv[i]));
            return 1;
        }
    }
    Application app{ enableDebug, inputPaths, worldsRoot };

	app.Run();
}