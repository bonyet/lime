#pragma once

enum class Type
{
	// Primitives
	Int = 1, Float, Boolean, 
	Void,
	String,
};

Type TypeFromString(const char* string);