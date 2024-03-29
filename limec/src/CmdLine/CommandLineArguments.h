#pragma once

/*
Jesus said to her, �I am the resurrection and the life.
The one who believes in me will live, even though they die;
and whoever lives by believing in me will never die.
Do you believe this?�

John 11:25-26
*/
struct CommandLineArguments
{
	// Whatever string is after "limec"
	const char* path;

	int optimizationLevel = 0;
	bool buildAndRun = false;

	static CommandLineArguments FromCommandLine(int argc, const char* argv[], std::string& errorOuput);
};