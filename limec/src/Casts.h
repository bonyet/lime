#pragma once

#include "Type.h"

struct Cast
{
	using ConversionFunc = llvm::Value*(*)(llvm::Value* from);

	Type* from, *to;
	bool implicit = false;
	ConversionFunc func = nullptr;

	Cast(Type* from, Type* to, ConversionFunc convert, bool implicit = false)
		: from(from), to(to), func(convert), implicit(implicit)
	{}

	llvm::Value* Try(llvm::Value* f)
	{
		return func(f);
	}

	static llvm::Value* TryIfValid(Type* from, Type* to, llvm::Value* value)
	{
		return TryIfValid(from->raw, to->raw, value);
	}
	static llvm::Value* TryIfValid(llvm::Type* from, llvm::Type* to, llvm::Value* value)
	{
		if (value->getType() == to)
			return value; // No need to cast

		if (Cast* cast = IsValid(from, to))
			return cast->Try(value);

		return nullptr;
	}
	static Cast* IsValid(Type* from, Type* to)
	{
		return IsValid(from->raw, to->raw);
	}
	static Cast* IsValid(llvm::Type* from, llvm::Type* to)
	{
		for (int i = 0; i < allowedImplicitCasts.size(); i++)
		{
			Cast& cast = allowedImplicitCasts[i];
			if (cast.from->raw == from && cast.to->raw == to && cast.implicit == true)
			{
				return &cast;
			}
		}
		return nullptr;
	}

	static std::vector<Cast> allowedImplicitCasts;
};