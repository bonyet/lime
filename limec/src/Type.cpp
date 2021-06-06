#include "Type.h"

#include "Error.h"

Type TypeFromString(const char* string)
{
	switch (*string)
	{
	case 'i':
		return Type::Int;
	case 'f':
		return Type::Float;
	case 'b':
		return Type::Boolean;
	case 's':
		return Type::String;
	}
	
	throw LimeError("Invalid type string");
}