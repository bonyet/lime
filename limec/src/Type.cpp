#include <string>

#include "Type.h"
#include <memory>

#include "Utils.h"
#include "Error.h"

#include "Typer.h"

Type* Type::int32Type = new Type
(
	"int"
);
Type* Type::floatType = new Type
(
	"float"
);
Type* Type::boolType = new Type
(
	"bool"
);
Type* Type::stringType = new Type
(
	"string"
);
Type* Type::voidType = new Type
(
	"void"
);

std::vector<Type*> Typer::definedTypes =
{
	Type::int32Type, Type::floatType, Type::boolType, Type::stringType, Type::voidType,
};

Type* Type::FromString(const char* string)
{
	switch (*string)
	{
	case 'i':
		return int32Type;
	case 'f':
		return floatType;
	case 'b':
		return boolType;
	case 's':
		return stringType;
	case 'v':
		return voidType; // Maybe not
	}
	
	return nullptr;
}