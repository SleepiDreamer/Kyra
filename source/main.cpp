#include <windows.h>
#include "Application.h"
#include "Log.h"

#include <windows.h>
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
    try
    {
        bool enableDebug = false;
        std::vector<std::string> inputPaths;

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
            else
            {
				Log::Warning("Unknown argument: {}", std::string(argv[i]));
                return 1;
            }
        }
        Application app{ enableDebug, inputPaths };

		app.Run();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
