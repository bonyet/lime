#pragma once

struct ScopedValue
{
	Type* type;
};

struct Scope
{
	bool isFunctionScope = false;
	std::unordered_map<std::string, ScopedValue> namedVariableTypes;
};