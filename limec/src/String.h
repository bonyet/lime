#pragma once

#include <memory>
#include <fstream>

// C-like helper function to check if a string contains a char up to a certain amount of characters
static bool strnchr(const char* string, const char c, int n)
{
	for (int i = 0; i < n; i++)
	{
		if (string[i] == c)
			return true;
	}

	return false;
}

class String
{
public:
	// Empty string
	String()
	{
		buffer = new char[1];
		buffer[0] = '\0';
	}
	String(const char* chars)
	{
		size = (uint32_t)strlen(chars);
		capacity = size * 2;
		buffer = new char[capacity + 1];
		strcpy(buffer, chars);
		buffer[size] = '\0';
	}
	String(const char* chars, int length)
	{
		Copy(chars, length);
	}
	String(const String& other)
	{
		Copy(other);
	}
	String(String&& other) noexcept
		: buffer(std::exchange(other.buffer, nullptr))
	{
		size = other.size;
		capacity = other.capacity;
	}
	~String()
	{
		size = 0;
		delete[] buffer;
	}

	// Copies the contents of other into this string
	void Copy(const String& other)
	{
		size = other.size;
		Reallocate(other.capacity);

		memcpy(buffer, other.buffer, size);
		buffer[size] = '\0';
	}
	
	// Copies n characters from chars into this string
	void Copy(const char* chars, int amount)
	{
		size = amount;
		Reallocate(size * 2);

		strncpy(buffer, chars, (size_t)amount);
		buffer[size] = '\0';
	}
	void Reserve(int newSize)
	{
		Reallocate(newSize);
	}

	template<typename It>
	void Assign(It start, It end)
	{
		for (It it = start; it != end; ++it)
		{
			buffer[size++] = *it;
		}
		buffer[size] = '\0';
	}

	template<typename... Args>
	void Format(const char* format, Args&&... args)
	{
		int sizeNeeded = snprintf(buffer, 0, format, std::forward<Args>(args)...);
		Reallocate(sizeNeeded * 2 + 1);
		size = sizeNeeded;
		sprintf(buffer, format, std::forward<Args>(args)...);
		buffer[size] = '\0';
	}

	template<typename... Args>
	static String FromFormat(const char* format, Args&&... args)
	{
		String string;
		string.Format(format, std::forward<Args>(args)...);
		return string;
	}

	// Operators
	void operator+=(char c)
	{
		if (size >= capacity)
			Reallocate(capacity * 2);

		buffer[size++] = c;
		buffer[size] = '\0';
	}
	void operator+=(const char* chars)
	{
		uint32_t newTotalSize = size + (uint32_t)strlen(chars);
		if (newTotalSize >= capacity)
			Reallocate(newTotalSize * 2 + 1); // Room for null char?

		buffer = strcat(buffer, chars);
		size = newTotalSize;
	}
	String operator+(const char* chars) const
	{
		String result;
		uint32_t totalSize = size + (uint32_t)strlen(chars);

		result.size = totalSize;
		result.capacity = totalSize * 2;

		char* temp = (char*)realloc(result.buffer, totalSize + 1);
		if (temp)
			result.buffer = temp;

		strcpy(result.buffer, buffer);
		strcat(result.buffer, chars);

		return result;
	}

	String& operator=(const String& other)
	{
		Copy(other);
		return *this;
	}

	operator const char* () const
	{
		return buffer;
	}
	const char* chars() const
	{
		return buffer;
	}
private:
	void Reallocate(uint32_t newCapacity)
	{
		capacity = newCapacity;
		char* temp = (char*)realloc(buffer, (size_t)capacity);
		if (temp)
			buffer = temp;
	}
private:
	char* buffer = nullptr;
	uint32_t size = 0;
	uint32_t capacity = 2;
};