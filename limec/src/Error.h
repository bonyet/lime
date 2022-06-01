#pragma once

// Error during parsing / lexing
struct LimeError : std::exception
{
	int GetLine();
	int GetColumn();

	template<typename... Args>
	LimeError(const char* format, Args&&... args)
		: line(GetLine()), column(GetColumn())
	{
		message = FormatString(format, std::forward<Args>(args)...);

		column = GetColumn();
	}

	int line = 0, column = 0;
	std::string message;
};

// Error during code gen
struct CompileError : std::exception
{
	template<typename... Args>
	CompileError(int line, const char* format, Args&&... args)
		: line(line), message(FormatString(format, std::forward<Args>(args)...))
	{
	}

	int line = 0;
	std::string message;
};