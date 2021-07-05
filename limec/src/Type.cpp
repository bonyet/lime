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

Type* Type::int32PtrType = new Type
(
	"*int"
);
Type* Type::floatPtrType = new Type
(
	"*float"
);
Type* Type::boolPtrType = new Type
(
	"*bool"
);
Type* Type::stringPtrType = new Type
(
	"*string"
);

std::vector<Type*> Typer::definedTypes =
{
	Type::int32Type,  Type::int32PtrType,
	Type::floatType,  Type::floatPtrType,
	Type::boolType,   Type::boolPtrType,
	Type::stringType, Type::stringPtrType,
	Type::voidType,
};