#pragma once

#include "Tree.h"

class Generator
{
public:
	Generator();
	~Generator();

	void Generate(std::unique_ptr<Compound> module);
};