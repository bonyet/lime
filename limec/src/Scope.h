#pragma once

struct ScopedValue
{
	Type type;
	bool global;
};

struct Scope
{
	std::unordered_map<std::string, ScopedValue> namedVariableTypes;
	//std::vector<Scope> childrenScopes;
};