#pragma once

#include "String.h"

// Error during parsing / lexing
struct LimeError : std::exception
{
	int GetLine();

	template<typename... Args>
	LimeError(const char* format, Args&&... args)
	{
		const String formatLine = String::FromFormat("[Line %d]: ", GetLine());
		message = formatLine + String::FromFormat(format, std::forward<Args>(args)...);
	}

	String message;
};

// Error during code gen
struct CompileError : std::exception
{
	template<typename... Args>
	CompileError(const char* format, Args&&... args)
	{
		message = String::FromFormat(format, std::forward<Args>(args)...);
	}

	String message;
};