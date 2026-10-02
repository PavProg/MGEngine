#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include "core/profiler.hpp"

bool Logger::s_initialized = false;
std::ofstream Logger::s_file;
static std::mutex s_logMutex;

void Logger::Init(const std::string& filename) {
    if (s_initialized) return;

    s_file.open(filename, std::ios::app);
    if (!s_file.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }

    s_initialized = true;
    Info("Logger initialized");
}

void Logger::ShutDown() {
    Info("Logger shutdown");
    if (s_file.is_open()) {
        s_file.close();
    }
    s_initialized = false;
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (!s_initialized) return;
    std::string levelStr;
    switch(level) {
        case LogLevel::Info: levelStr = "INFO"; break; 
        case LogLevel::Warning: levelStr = "WARNING"; break;
        case LogLevel::Error: levelStr = "ERROR"; break;
    }
    std::string formatted = "[" + GetCurrentTime() + "][" + levelStr + "] " + message;

    MGE_PROFILE_MESSAGE(formatted.c_str(), formatted.size());

    // std::cout << formatted << std::endl;

    if (s_file.is_open()) {
        s_file << formatted << std::endl;
        s_file.flush();
    }

}

void Logger::Info(const std::string& message) {
    Log(LogLevel::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::Error, message);
}

std::string Logger::GetCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    char buf[20];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &bt);
    return std::string(buf);
}