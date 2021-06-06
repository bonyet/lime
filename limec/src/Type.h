#pragma once

enum class Type
{
	// Primitives
	Int = 1, Float, Boolean, 
	String,
};

Type TypeFromString(const char* string);