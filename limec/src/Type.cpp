#include "limecpch.h"

#include "Type.h"
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