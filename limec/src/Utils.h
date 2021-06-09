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