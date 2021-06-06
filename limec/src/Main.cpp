#include <iostream>
#include "String.h"
#include "Parser.h"

#include <chrono>
#include <fstream>

#include "Generator.h"

static String ReadFile(const char* filepath)
{
	String fileContents;

	// Read file
	std::ifstream stream(filepath);

	if (!stream.good())
	{
		fprintf(stderr, "Failed to open file \"%s\".\n", filepath);
		return fileContents;
	}

	stream.seekg(0, std::ios::end);
	fileContents.Reserve((int)stream.tellg() + 1); // Room for null-terminating character
	stream.seekg(0, std::ios::beg);

	fileContents.Assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

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
	String contents = ReadFile(mainPath);
	
	Lexer lexer = Lexer(contents.chars());
	Parser parser;

	ParseResult parseResult = parser.Parse(&lexer);
	printf(parseResult.Succeeded ? "Parsing succeeded\n" : "Parsing failed\n");
	printf("\n");

	if (parseResult.Succeeded)
	{
		Generator generator;
		generator.Generate(std::move(parseResult.module));
	}
}