#pragma once

#include "Typer.h"

struct Type
{
	static Type *int8Type, *int32Type, *int64Type, *floatType, *boolType, *stringType, *voidType;
	static Type *int8PtrType, *int32PtrType, *int64PtrType, *floatPtrType, *boolPtrType, *stringPtrType;

	std::string name;
	llvm::Type* raw = nullptr;

	Type() = default;
	Type(const std::string& name)
		: name(name) {}
	Type(const std::string& name, llvm::Type* raw)
		: name(name), raw(raw) {}

	bool operator==(const Type* other) const { return this == other; }
	bool operator!=(const Type* other) const { return this != other; }

	bool isInt()    const { return this == int8Type || this == int32Type || this == int64Type; }
	bool isInt8()   const { return this == int8Type;   }
	bool isInt32()  const { return this == int32Type;  }
	bool isInt64()  const { return this == int64Type;  }
	bool isFloat()  const { return this == floatType;  }
	bool isBool()   const { return this == boolType;   }
	bool isString() const { return this == stringType; }
	bool isVoid()   const { return this == voidType;   }

	bool isIntPtr()    const { return this == int8PtrType || this == int32PtrType || this == int64PtrType; }
	bool isInt8Ptr()   const { return this == int8PtrType;   }
	bool isInt32Ptr()  const { return this == int32PtrType;  }
	bool isInt64Ptr()  const { return this == int64PtrType;  }
	bool isFloatPtr()  const { return this == floatPtrType;  }
	bool isBoolPtr()   const { return this == boolPtrType;   }
	bool isStringPtr() const { return this == stringPtrType; }

	bool isPointer()   const { return name[0] == '*'; }
	bool isPointerTo(Type* type) const
	{
		return name.substr(1, name.length() - 1) == type->name;
	}
	Type* getTypePointedTo() const
	{
		if (!isPointer())
			return nullptr;

		return Typer::Get(name.substr(1, name.length() - 1));
	}

	virtual bool isPrimitive() const { return true; }
private:
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