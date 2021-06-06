#include "Generator.h"

#include "Error.h"
#include "Type.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static std::map<std::string, llvm::Value*> namedValues;

// Utils
static llvm::Type* GetLLVMType(Type type)
{
	switch (type)
	{
	case Type::Int:
		return llvm::Type::getInt32Ty(*context);
	case Type::Float:
		return llvm::Type::getFloatTy(*context);
	case Type::Boolean:
		return llvm::Type::getInt1Ty(*context); // A bool is just small int !
	case Type::Void:
		return llvm::Type::getVoidTy(*context);
	}

	throw CompileError("Unknown type");
	return nullptr;
}

void* PrimaryValue::Generate()
{
	switch (type)
	{
	case Type::Int:
		return llvm::ConstantInt::get(*context, llvm::APInt(32, value.i32));
	case Type::Float:
		return llvm::ConstantFP::get(*context, llvm::APFloat(value.f32));
	case Type::Boolean:
		return llvm::ConstantInt::getBool(*context, value.b32);
	}
}

void* VariableDefinition::Generate()
{
	using namespace llvm;

	// Allocate on the stack
	AllocaInst* allocaInst = builder->CreateAlloca(GetLLVMType(type), nullptr, name.chars());
	StoreInst* storeInst = builder->CreateStore((Value*)initializer->Generate(), allocaInst, false);
	namedValues.emplace(name.chars(), allocaInst);

	return allocaInst;
}

void* Variable::Generate()
{
	using namespace llvm;

	Value* value = namedValues[name.chars()];
	if (!value)
		throw CompileError("Unknown variable name");

	return builder->CreateLoad(namedValues[name.chars()], name.chars());
}

void* Binary::Generate()
{
	llvm::Value* lhs = (llvm::Value*)left->Generate();
	llvm::Value* rhs = (llvm::Value*)right->Generate();

	if (!lhs || !rhs)
		throw CompileError("Invalid binary operator"); // What happon

	if (right->type != left->type)
		throw CompileError("Both operands of a binary operation must be of the same type");

	Type opType = left->type;
	
	// please find me a better way of doing this
	switch (binaryType)
	{
	case BinaryType::Add:
	{
		if (opType == Type::Int)
			return builder->CreateAdd(lhs, rhs, "addtmp");
		if (opType == Type::Float)
			return builder->CreateFAdd(lhs, rhs, "addtmp");
		break;
	}
	case BinaryType::Subtract:
	{
		if (opType == Type::Int)
			return builder->CreateSub(lhs, rhs, "subtmp");
		if (opType == Type::Float)
			return builder->CreateFSub(lhs, rhs, "subtmp");
		break;
	}
	case BinaryType::Multiply:
	{
		if (opType == Type::Int)
			return builder->CreateMul(lhs, rhs, "multmp");
		if (opType == Type::Float)
			return builder->CreateFMul(lhs, rhs, "multmp");
		break;
	}
	case BinaryType::Divide:
	{
		if (opType == Type::Int)
			throw CompileError("Integer division not supported");
		if (opType == Type::Float)
			return builder->CreateFDiv(lhs, rhs, "divtmp");
		break;
	}
	case BinaryType::Equal:
	{
		if (opType == Type::Int)
			return builder->CreateICmpEQ(lhs, rhs, "cmptmp");
		if (opType == Type::Float)
			return builder->CreateFCmpUEQ(lhs, rhs, "cmptmp");
		break;
	}
	case BinaryType::Less:
	{
		if (opType == Type::Int)
			return builder->CreateICmpULT(lhs, rhs, "cmptmp");
		if (opType == Type::Float)
			return builder->CreateFCmpULT(lhs, rhs, "cmptmp");
		break;
	}
	case BinaryType::LessEqual:
	{
		if (opType == Type::Int)
			return builder->CreateICmpULE(lhs, rhs, "cmptmp");
		if (opType == Type::Float)
			return builder->CreateFCmpULE(lhs, rhs, "cmptmp");
		break;
	}
	case BinaryType::Greater:
	{
		if (opType == Type::Int)
			return builder->CreateICmpUGT(lhs, rhs, "cmptmp");
		if (opType == Type::Float)
			return builder->CreateFCmpUGT(lhs, rhs, "cmptmp");
		break;
	}
	case BinaryType::GreaterEqual:
	{
		if (opType == Type::Int)
			return builder->CreateICmpUGE(lhs, rhs, "cmptmp");
		if (opType == Type::Float)
			return builder->CreateFCmpUGE(lhs, rhs, "cmptmp");
		break;
	}
	default:
		throw CompileError("Invalid binary operator");
	}
}

void* Call::Generate()
{
	using namespace llvm;

	// Look up function name
	Function* func = module->getFunction(fnName.chars());
	if (!func)
		throw CompileError("Unknown function referenced");

	// Handle arg mismatch
	if (func->arg_size() != args.size())
		throw CompileError("Incorrect number of arguments passed to '%s'", fnName.chars());

	// Generate arguments
	std::vector<Value*> argValues;
	for (auto& expression : args)
	{
		Value* generated = (Value*)expression->Generate();
		argValues.push_back(generated);
		if (!generated)
		{
			throw CompileError("Failed to generate function argument");
		}
	}

	return (Value*)builder->CreateCall(func, argValues, "calltmp");
}

void* FunctionDefinition::Generate()
{
	llvm::Function* function = module->getFunction(name.chars());
	// Create function if it doesn't exist
	if (!function)
	{
		std::vector<llvm::Type*> paramTypes(params.size());

		// Fill paramTypes with proper types
		int index = 0;
		for (auto& param : params)
			paramTypes[index++] = GetLLVMType(param.type);
		
		index = 0; // Reuse later

		llvm::Type* returnType = GetLLVMType(type);

		if (!returnType)
			throw CompileError("Invalid return type for '%s'", name.chars());

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name.chars(), *module);

		// Set names for function args
		for (auto& arg : function->args())
		{
			arg.setName(params[index++].name.chars());
		}
	}
	if (!function->empty())
	{
		throw CompileError("Function cannot be redefined");
	}

	// Setup function body

	// Create block to start insertion into
	llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(block);

	// Record args into values map
	namedValues.clear();
	for (auto& arg : function->args())
	{
		namedValues[arg.getName().str()] = &arg;
	}

	// Generate body
	for (auto& statement : body)
	{
		statement->Generate();
	}

	// Return value
	llvm::Value* returnValue = indexOfReturnInBody != -1 ? (llvm::Value*)body[indexOfReturnInBody]->Generate() : nullptr;
	builder->CreateRet(returnValue);

	if (verifyFunction(*function, &llvm::errs()))
	{
		function->eraseFromParent();
		printf("\n");
	}

	return function;
}

Generator::Generator()
{
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>("Code Module", *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

Generator::~Generator()
{
}

void Generator::Generate(std::unique_ptr<struct Compound> compound)
{
	try
	{
		for (auto& child : compound->statements)
		{
			child->Generate();
		}
	}
	catch (CompileError& err)
	{
		fprintf(stderr, "CodeGenError: %s\n\n", err.message.chars());
	}

	module->print(llvm::errs(), nullptr);
}