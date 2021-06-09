#pragma once

struct Scope
{
	std::unordered_map<std::string, Type> namedVariableTypes;
	//std::vector<Scope> childrenScopes;
};