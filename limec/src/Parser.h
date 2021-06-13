#pragma once

#include "Lexer.h"

struct ParseResult
{
	bool Succeeded = true;
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

	uint32_t scopeDepth = 0;

	ParseState state = ParseState::Default;
	Token current;
	Lexer* lexer = nullptr;
	struct Scope* scope;
};