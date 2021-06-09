#pragma once

// Error during parsing / lexing
struct LimeError : std::exception
{
	int GetLine();

	template<typename... Args>
	LimeError(const char* format, Args&&... args)
	{
		const auto formatLine = FormatString("[Line %d]: ", GetLine());
		message = formatLine + FormatString(format, std::forward<Args>(args)...);
	}

	std::string message;
};

// Error during code gen
struct CompileError : std::exception
{
	template<typename... Args>
	CompileError(const char* format, Args&&... args)
		: message(FormatString(format, std::forward<Args>(args)...))
	{
	}

	std::string message;
};