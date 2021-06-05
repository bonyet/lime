#include "Generator.h"

#include "Error.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

static std::unique_ptr<LLVMContext> context;
static std::unique_ptr<IRBuilder<>> builder;
static std::unique_ptr<Module> module;
static std::map<String, Value*> namedValues;

Value* PrimaryNumber::Generate()
{
	return ConstantFP::get(*context, APFloat(value.f32));
}

Value* Variable::Generate()
{
	Value* value = namedValues[idName];
	if (!value)
		throw LimeError("Unknown variable name");

	return value;
}

Value* Binary::Generate()
{
	Value* lhs = left->Generate();
	Value* rhs = right->Generate();

	if (!lhs || !rhs)
		return nullptr;

	switch (type)
	{
	case BinaryType::Add:
		return builder->CreateFAdd(lhs, rhs, "addtmp");
	case BinaryType::Subtract:
		return builder->CreateFSub(lhs, rhs, "subtmp");
	case BinaryType::Multiply:
		return builder->CreateFMul(lhs, rhs, "multmp");
	case BinaryType::Divide:
		return builder->CreateFDiv(lhs, rhs, "divtmp");
	case BinaryType::Equal:
		return builder->CreateFCmpUEQ(lhs, rhs, "cmptmp");
	case BinaryType::Less:
		return builder->CreateFCmpULT(lhs, rhs, "cmptmp");
	case BinaryType::LessEqual:
		return builder->CreateFCmpULE(lhs, rhs, "cmptmp");
	case BinaryType::Greater:
		return builder->CreateFCmpUGT(lhs, rhs, "cmptmp");
	case BinaryType::GreaterEqual:
		return builder->CreateFCmpUGE(lhs, rhs, "cmptmp");
	default:
		throw LimeError("Invalid binary operator");
	}
}

Generator::Generator()
{
	context = std::make_unique<LLVMContext>();
	module = std::make_unique<Module>("Module", *context);
	builder = std::make_unique<IRBuilder<>>(*context);
}

Generator::~Generator()
{
}

void Generator::Generate(std::unique_ptr<struct Compound> compound)
{
	for (auto& child : compound->statements)
	{
		Value* value = child->Generate();
	}

	module->print(errs(), nullptr);
}