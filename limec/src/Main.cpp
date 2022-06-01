#include "limecpch.h"

#include <llvm/IR/Value.h>

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

#define COMPILATION_OUTPUT 0

int main(int argc, const char* argv[])
{
	PROFILE_BEGIN_SESSION("Profile", "ProfileResult.json");

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
		auto result = generator.Generate(parseResult);

		if (!result.Succeeded)
			return 0;

		Emitter emitter;
		emitter.Emit(result.ir, "result.ll");

		LaunchProcess("lli result.ll");

#if COMPILATION_OUTPUT
	#ifdef _WIN32
		LaunchProcess("\"llvm-as\" result.ll");
		// Generate obj file
		LaunchProcess("\"llc\" result.bc");
		// Link
		//LaunchProcess("\"link\" result.o -defaultlib:libcmt");
	#else
		#error "Platform not supported"
	#endif
#endif
	}

	PROFILE_END_SESSION();
}