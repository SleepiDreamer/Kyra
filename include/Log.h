#pragma once
#include <Windows.h>
#include <iostream>
#include <format>

class Log
{
public:
    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args)
    {
        Print(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Success(std::format_string<Args...> fmt, Args&&... args)
    {
        Print(FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warning(std::format_string<Args...> fmt, Args&&... args)
    {
        Print(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args)
    {
        Print(FOREGROUND_RED | FOREGROUND_INTENSITY,
            std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Critical(std::format_string<Args...> fmt, Args&&... args)
    {
        Print(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_RED,
            std::format(fmt, std::forward<Args>(args)...));
        __debugbreak();
    }

private:
    static void Print(WORD color, const std::string& message)
    {
        static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(console, color);
        std::cout << message << "\n";
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
};