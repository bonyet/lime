#include "TreePrinter.h"

static void PrintCompound(Compound* compound)
{
	int indent = 0;
	printf("%s", compound->ToString(indent).c_str());
}

void PrintStatement(Statement* statement)
{
	printf("\n");

	if (dynamic_cast<Compound*>(statement))
	{
		PrintCompound((Compound*)statement);
	}
	else
	{
		int indent = 0;
		printf("%s", statement->ToString(indent).c_str());
	}

	printf("\n\n");
}