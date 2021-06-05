#pragma once

#include "String.h"

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