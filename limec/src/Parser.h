#pragma once

#include "Lexer.h"

struct ParseResult
{
	bool Succeeded = false;
	std::unique_ptr<Compound> module;
};

enum class ParseState : int
{
	Default       = 0 << 0,
	Expression    = 1 << 0,
	VariableWrite = 1 << 1,
	FuncCallArgs  = 1 << 2,
};

inline int operator&(ParseState a, ParseState b)
{
	return (int)a & (int)b;
}
inline ParseState operator|(ParseState a, ParseState b)
{
	return (ParseState)((int)a | (int)b);
}
inline ParseState operator|=(ParseState& a, ParseState b)
{
	return (a = (ParseState)((int)a | (int)b));
}

struct Parser
{
	ParseResult Parse(Lexer* lexer);

	Token current;
	struct Scope* scope;
	Lexer* lexer = nullptr;
	uint32_t scopeDepth = 0;
	ParseState state = ParseState::Default;
};