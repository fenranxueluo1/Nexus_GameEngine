#pragma once
#include <string>
#include <string_view>
#include <source_location>
#include <vector>
#include <cassert>

/*
* @brief 记录信息日志的可变参数宏。该宏接收一个字符串消息，后面跟必要的参数。
* @param 接收 std::string_view 或字符串，形如 "This is a log value: {0}, and {1}"，后面跟参数
*/
#define NEXUS_LOG(...) NEXUS_LOGGER::Logger::GetInstance().Log(__VA_ARGS__);

/*
* @brief 记录警告日志的可变参数宏。该宏接收一个字符串消息，后面跟必要的参数。
* @param 接收 std::string_view 或字符串，形如 "This is a log value: {0}, and {1}"，后面跟参数
*/
#define NEXUS_WARN(...) NEXUS_LOGGER::Logger::GetInstance().Warn(__VA_ARGS__);

/*
* @brief 记录错误日志的可变参数宏。该宏接收一个字符串消息，后面跟必要的参数。
* @param 接收 std::string_view 或字符串，形如 "This is a log value: {0}, and {1}"，后面跟参数
*/
#define NEXUS_ERROR(...) NEXUS_LOGGER::Logger::GetInstance().Error(std::source_location::current(), __VA_ARGS__);
#define NEXUS_INIT_LOGS(console, retain) NEXUS_LOGGER::Logger::GetInstance().Init(console, retain);

namespace NEXUS_LOGGER {

	struct LogEntry
	{
		enum class LogType { INFO, WARN, ERR, NONE };
		LogType type{ LogType::INFO };
		std::string log{""};
	};

	class Logger
	{
	private:
		std::vector<LogEntry> m_LogEntries;
		bool m_bLogAdded{ false }, m_bInitialized{ false }, m_bConsoleLog{ true }, m_bRetainLogs{ true };

		Logger() = default;

		struct LogTime
		{
			std::string day, dayNumber, month, year, time;
			LogTime(const std::string& date);
		};

		std::string CurrentDateTime();

	public:
		static Logger& GetInstance();
		
		~Logger() = default;
		// 禁止拷贝，保证全局唯一
		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;


		void Init(bool consoleLog = true, bool retainLogs = true);

		template <typename... Args>
		void Log(const std::string& message, Args&&... args);

		template <typename... Args>
		void Warn(const std::string& message, Args&&... args);

		template <typename... Args>
		void Error(std::source_location location, const std::string& message, Args&&... args);

	};
}

#include "Logger.inl"
