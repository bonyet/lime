#include "limecpch.h"

#include "PlatformUtils.h"

#include "Tree.h"

#include "Generator.h"
#include "Emitter.h"

static std::string ReadFile(const char* filepath)
{
	std::string fileContents;

	// Read file
	std::ifstream stream(filepath);

	if (!stream.good())
	{
		fprintf(stderr, "failed to open file \"%s\".\n", filepath);
		return fileContents;
	}

	stream.seekg(0, std::ios::end);
	fileContents.reserve((int)stream.tellg() + 1); // Room for null-terminating character
	stream.seekg(0, std::ios::beg);

	fileContents.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

	stream.close();

	return fileContents;
}

int main(int argc, const char* argv[])
{
	PROFILE_BEGIN_SESSION("Profile", "ProfileResult.json");

	// If we are running from the cmd line:
#ifdef LIMEC_RELEASE
	if (argc == 1)
	{
		fprintf(stderr, "Usage: limec <path>\n");
		return 1;
	}

	std::string cmdParseError;
	CommandLineArguments arguments = CommandLineArguments::FromCommandLine(argc, argv, cmdParseError);
	if (!cmdParseError.empty())
	{
		fprintf(stderr, "%s\n", cmdParseError.c_str());
		return 1;
	}
	
	std::string mainPath = arguments.path;
#elif LIMEC_DEBUG
	const char* argV[] = { "limec", "main.lm", };
	int argC = std::size(argV);

	std::string cmdParseError;
	CommandLineArguments arguments = CommandLineArguments::FromCommandLine(argC, argV, cmdParseError);
	if (!cmdParseError.empty())
	{
		fprintf(stderr, "%s\n", cmdParseError.c_str());
		return 1;
	}

	std::string mainPath = arguments.path;
#endif
	if (mainPath.substr(mainPath.length() - 2, 2) != "lm")
	{
		fprintf(stderr, "expected a .lm file\n");
		return 1;
	}
	std::string contents = ReadFile(mainPath.c_str());
	
	Lexer lexer = Lexer(contents.c_str());
	Parser parser;

	ParseResult parseResult = parser.Parse(&lexer);
	printf("\n");

	if (parseResult.Succeeded)
	{
		Generator generator;
		auto result = generator.Generate(parseResult, arguments);

		if (!result.Succeeded)
			return 0;

		std::string outputPath = mainPath.substr(0, mainPath.size() - 2) + "ll";

		Emitter emitter;
		emitter.Emit(result.ir, outputPath.c_str());

		if (arguments.buildAndRun)
		{
			LaunchProcess(FormatString("lime %s", outputPath.c_str()).c_str());
		}
	}

	PROFILE_END_SESSION();
}