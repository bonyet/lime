#pragma once

struct ScopedValue
{
	Type* type;
	VariableFlags flags;
};

struct Scope
{
	std::unordered_map<std::string, ScopedValue> namedVariableTypes;
};