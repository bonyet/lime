#include <iostream>
#include "Parser.h"

#include <chrono>
#include <fstream>

#include "Generator.h"

static std::string ReadFile(const char* filepath)
{
	std::string fileContents;

	// Read file
	std::ifstream stream(filepath);

	if (!stream.good())
	{
		fprintf(stderr, "Failed to open file \"%s\".\n", filepath);
		return fileContents;
	}

	stream.seekg(0, std::ios::end);
	fileContents.reserve((int)stream.tellg() + 1); // Room for null-terminating character
	stream.seekg(0, std::ios::beg);

	fileContents.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

	return fileContents;
}

int main(int argc, const char* argv[])
{
	// If we are running from the cmd line:
#if 0
	if (argc == 1)
	{
		fprintf(stderr, "Usage: lime <path>\n");
		return 1;
	}
	
	const char* mainPath = argv[1];
#endif

	const char* mainPath = "main.lm";
	std::string contents = ReadFile(mainPath);
	
	Lexer lexer = Lexer(contents.c_str());
	Parser parser;

	ParseResult parseResult = parser.Parse(&lexer);
	printf("\n");

	if (parseResult.Succeeded)
	{
		Generator generator;
		generator.Generate(std::move(parseResult.module));
	}
}