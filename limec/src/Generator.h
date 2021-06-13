#pragma once

class Generator
{
public:
	Generator();

	void Generate(std::unique_ptr<Compound> module);
};