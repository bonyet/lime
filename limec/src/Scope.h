#pragma once

struct ScopedValue
{
	Type* type;
};

struct Scope
{
	std::unordered_map<std::string, ScopedValue> namedVariableTypes;
};