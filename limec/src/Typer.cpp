#include "limecpch.h"
#include "Type.h"
#include "Typer.h"

Type* Typer::Get(const std::string& typeName)
{
	Type* result = nullptr;

	// Make sure we have registered the type
	for (Type* type : definedTypes)
	{
		if (type->name == typeName)
		{
			result = type;
		}
	}

	if (!result)
		throw LimeError("type '%s' not registered\n", typeName.c_str());

	return result;
}

bool Typer::Exists(const std::string& typeName)
{
	for (Type* type : definedTypes)
	{
		if (type->name == typeName)
			return true;
	}

	return false;
}

void Typer::Release()
{
	for (Type* type : definedTypes)
	{
		delete type;
	}

	definedTypes.clear();
}
