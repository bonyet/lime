#pragma once

#include <vector>
#include <llvm/IR/Type.h>

struct Type
{
	static Type* int32Type, *floatType, *boolType, *stringType, *voidType;

	std::string name;
	llvm::Type* raw = nullptr;

	Type() = default;
	Type(const std::string& name)
		: name(name) {}
	Type(const std::string& name, llvm::Type* raw)
		: name(name), raw(raw) {}

	bool operator==(const Type* other) const { return this == other; }
	bool operator!=(const Type* other) const { return this != other; }

	bool isInt()    const { return this == int32Type; }
	bool isFloat()  const { return this == floatType; }
	bool isBool()   const { return this == boolType; }
	bool isString() const { return this == stringType; }
	bool isVoid()   const { return this == voidType; }
	bool isPrimitive() const
	{
		return isInt() || isFloat() || isString() || isBool() || isVoid();
	}

	static Type* FromString(const char* string);

	friend class Typer;
	friend struct UserDefinedType;
}; 

struct UserDefinedType : public Type
{
public:
	std::string name;
	std::vector<Type*> memberTypes;

	UserDefinedType() = default;
	UserDefinedType(const std::string& name)
		: name(name) {}
};