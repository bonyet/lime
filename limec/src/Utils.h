#pragma once

#include <forward_list>

template<typename... Args>
static std::string FormatString(const char* format, Args&&... args)
{
	auto size = (size_t)snprintf(nullptr, 0, format, args ...) + 1; // Room for null terminating char
	char* buffer = new char[size];
	std::snprintf(buffer, size, format, std::forward<Args>(args)...);

	std::string result{ buffer, buffer + size - 1 };

	delete[] buffer;

	return result;
}

// check if a string contains a char up to a certain amount of characters
static bool strnchr(const char* string, const char c, int n)
{
	for (int i = 0; i < n; i++)
	{
		if (string[i] == c)
			return true;
	}

	return false;
}