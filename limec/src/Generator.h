#pragma once

class Generator
{
public:
	Generator();

	std::string Generate(std::unique_ptr<Compound> module);
};