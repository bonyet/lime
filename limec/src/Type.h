#pragma once

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

	bool isInt()    const { return this == int32Type;  }
	bool isFloat()  const { return this == floatType;  }
	bool isBool()   const { return this == boolType;   }
	bool isString() const { return this == stringType; }
	bool isVoid()   const { return this == voidType;   }
	virtual bool isPrimitive() const { return true;   }

	friend class Typer;
	friend struct UserDefinedType;
}; 

struct UserDefinedType : public Type
{
public:
	std::vector<std::pair<std::string, Type*>> members;

	UserDefinedType() = default;
	UserDefinedType(const std::string& name)
		: Type(name) {}

	bool isPrimitive() const override { return false; }
};