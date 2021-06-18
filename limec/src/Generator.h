#pragma once

#include "Parser.h"

struct CompileResult
{
	std::string ir;
	bool Succeeded = true;
};

class Generator
{
public:
	Generator();

	CompileResult Generate(ParseResult& parseResult);
};