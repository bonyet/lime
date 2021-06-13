#include <string>

#include "Emitter.h"
#include <fstream>

void Emitter::Emit(const std::string& ir, const char* filepath)
{
	std::ofstream outStream(filepath);

	outStream.write(ir.c_str(), ir.size());

	outStream.close();
}