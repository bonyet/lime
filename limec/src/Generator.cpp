#include <string>
#include "Utils.h"

#include "Error.h"

#include "Generator.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::Function* currentFunction = nullptr;

static std::unordered_map<std::string, llvm::Value*> namedValues;

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
		return llvm::Type::getInt1Ty(*context);
	case Type::Void:
		return llvm::Type::getVoidTy(*context);
	}

	throw CompileError("Unknown type");
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
	AllocaInst* allocaInst = builder->CreateAlloca(GetLLVMType(type), nullptr, name);
	namedValues[name] = allocaInst;
	Value* value = (Value*)initializer->Generate();
	StoreInst* storeInst = builder->CreateStore(value, allocaInst);

	return allocaInst;
}

void* Variable::Generate()
{
	using namespace llvm;

	Value* value = namedValues[name];
	if (!value)
		throw CompileError("Unknown variable name '%s'", name.c_str());

	return builder->CreateLoad(value, name);
}

void* Binary::Generate()
{
	llvm::Value* lhs = (llvm::Value*)left->Generate();
	llvm::Value* rhs = (llvm::Value*)right->Generate();

	if (!lhs || !rhs)
		throw CompileError("Invalid binary operator"); // What happon

	// TODO: correct types for variables
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

void* Branch::Generate()
{
	using namespace llvm;

	BasicBlock* parentBlock = builder->GetInsertBlock();

	BasicBlock* trueBlock = BasicBlock::Create(*context, "btrue", currentFunction);
	BasicBlock* falseBlock = BasicBlock::Create(*context, "bfalse", currentFunction);

	// Create branch thing
	BranchInst* branch = builder->CreateCondBr((Value*)expression->Generate(), trueBlock, falseBlock);

	BasicBlock* endBlock = BasicBlock::Create(*context, "end", currentFunction);
	// Generate bodies
	{
		builder->SetInsertPoint(trueBlock);
		// Generate true body
		for (auto& statement : ifBody)
		{
			statement->Generate();
		}

		// Add jump to end
		builder->CreateBr(endBlock);

		// Generate false body
		builder->SetInsertPoint(falseBlock);
		for (auto& statement : elseBody)
		{
			statement->Generate();
		}

		// Add jump to end
		builder->CreateBr(endBlock);
	}

	builder->SetInsertPoint(parentBlock); // Reset to correct insert point

	// Insert end block
	builder->SetInsertPoint(endBlock);

	return branch;
}

void* Call::Generate()
{
	using namespace llvm;

	// Look up function name
	Function* func = module->getFunction(fnName);
	if (!func)
		throw CompileError("Unknown function referenced");

	// Handle arg mismatch
	if (func->arg_size() != args.size())
		throw CompileError("Incorrect number of arguments passed to '%s'", fnName.c_str());

	// Generate arguments
	std::vector<Value*> argValues;
	for (auto& expression : args)
	{
		Value* generated = (Value*)expression->Generate();
		if (!generated)
			throw CompileError("Failed to generate function argument");
		
		argValues.push_back(generated);
	}

	return builder->CreateCall(func, argValues);
}

static void GenerateEntryBlockAllocas(llvm::Function* function)
{
	using namespace llvm;

	for (auto& arg : function->args())
	{
		llvm::Type* type = arg.getType();

		if (arg.hasAttribute(Attribute::ByRef))
		{
			type = type->getPointerTo();
		}

		auto name = arg.getName(); // Not free!
		name = name.drop_back(1); // Remove arg suffix

		AllocaInst* allocaInst = builder->CreateAlloca(type, nullptr, name);
		namedValues[name.str()] = allocaInst;
	}
}

void* FunctionDefinition::Generate()
{
	llvm::Function* function = module->getFunction(name.c_str());
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
			throw CompileError("Invalid return type for '%s'", name.c_str());

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name.c_str(), *module);

		// Set names for function args
		for (auto& arg : function->args())
		{
			arg.setName(params[index++].name + '_'); // Suffix arguments with '_' to make stuff work
		}
	}
	if (!function->empty())
	{
		throw CompileError("Function cannot be redefined");
	}

	currentFunction = function;

	// Create block to start insertion into
	llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(block);

	// Allocate args
	namedValues.clear();
	GenerateEntryBlockAllocas(function);

	// Generate body
	int index = 0;
	for (auto& statement : body)
	{
		if (index++ == indexOfReturnInBody)
			continue; // Don't generate return statements twice

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
	module = std::make_unique<llvm::Module>("Module", *context);
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
		fprintf(stderr, "CodeGenError: %s\n\n", err.message.c_str());
	}

	module->print(llvm::errs(), nullptr);
}
