#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace Puluo {

class Log {
public:
    static void Init();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger()  { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
};

} // namespace Puluo

// Core log macros
#define PULUO_CORE_TRACE(...)    ::Puluo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define PULUO_CORE_INFO(...)     ::Puluo::Log::GetCoreLogger()->info(__VA_ARGS__)
#define PULUO_CORE_WARN(...)     ::Puluo::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define PULUO_CORE_ERROR(...)    ::Puluo::Log::GetCoreLogger()->error(__VA_ARGS__)
#define PULUO_CORE_CRITICAL(...) ::Puluo::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define PULUO_TRACE(...)    ::Puluo::Log::GetClientLogger()->trace(__VA_ARGS__)
#define PULUO_INFO(...)     ::Puluo::Log::GetClientLogger()->info(__VA_ARGS__)
#define PULUO_WARN(...)     ::Puluo::Log::GetClientLogger()->warn(__VA_ARGS__)
#define PULUO_ERROR(...)    ::Puluo::Log::GetClientLogger()->error(__VA_ARGS__)
#define PULUO_CRITICAL(...) ::Puluo::Log::GetClientLogger()->critical(__VA_ARGS__)
