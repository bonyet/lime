#pragma once

#include "Lexer.h"
#include "Tree.h"

struct ParseResult
{
	bool Succeeded = true;
	std::unique_ptr<Compound> module;
};

struct Parser
{
	enum class State
	{
		Default,

		VariableInitializer,
		FunctionCallArgs,
	};

	ParseResult Parse(Lexer* lexer);

	uint32_t scopeDepth = 0;

	State state = State::Default;
	Token current;
	Lexer* lexer = nullptr;
};