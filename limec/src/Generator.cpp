#include <string>
#include "Utils.h"

#include "Error.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Value.h>

#include "Tree.h"
#include "Generator.h"

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::Function* currentFunction = nullptr;

#define Assert(cond, msg) { if (!(cond)) { throw CompileError(msg); } }

struct NamedValue
{
	llvm::Value* raw = nullptr;
	VariableFlags flags = VariableFlags_None;
};

static std::unordered_map<std::string, NamedValue> namedValues;

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

llvm::Value* PrimaryValue::Generate()
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

static void ResetStackValues()
{
	for (auto it = namedValues.begin(); it != namedValues.end(); )
	{
		if (!(it->second.flags & VariableFlags_Global))
		{
			namedValues.erase(it++);
			continue;
		}

		++it;
	}
}

llvm::Value* VariableDefinition::Generate()
{
	using namespace llvm;
	
	llvm::Type* type = GetLLVMType(this->type);
	if (scope == 0)
	{
		// Global varable
		module->getOrInsertGlobal(name, type);
		GlobalVariable* gVar = module->getNamedGlobal(name);
		gVar->setLinkage(GlobalValue::CommonLinkage);

		if (initializer)
			gVar->setInitializer((Constant*)initializer->Generate());

		namedValues[name] = { gVar, flags };

		return gVar;
	}

	// Allocate on the stack
	AllocaInst* allocaInst = builder->CreateAlloca(type, nullptr, name);
	namedValues[name] = { allocaInst, flags };

	if (initializer)
	{
		Value* value = (Value*)initializer->Generate();
		StoreInst* storeInst = builder->CreateStore(value, allocaInst);
	}

	return allocaInst;
}

llvm::Value* VariableRead::Generate()
{
	using namespace llvm;

	// TODO: enforce shadowing rules

	if (!namedValues.count(name))
		throw CompileError("Unknown variable '%s'", name.c_str());

	NamedValue& value = namedValues[name];
	return builder->CreateLoad(value.raw, "loadtmp");
}

llvm::Value* VariableWrite::Generate()
{
	using namespace llvm;

	// TODO: enforce shadowing rules

	if (!namedValues.count(name))
		throw CompileError("Unknown variable '%s'", name.c_str());

	NamedValue& namedValue = namedValues[name];
	
	Value* value = right->Generate();
	return builder->CreateStore(value, namedValue.raw, "storetmp");
}

static llvm::Value* CreateBinOp(llvm::Value* left, llvm::Value* right, 
	BinaryType type, Type lType, VariableFlags lFlags, VariableFlags rFlags)
{
	using llvm::Instruction;

	Instruction::BinaryOps instruction = (Instruction::BinaryOps)-1;
	switch (type)
	{
		case BinaryType::CompoundAdd:
			Assert(lFlags & VariableFlags_Mutable, "Cannot assign to an immutable entity");
		case BinaryType::Add:
		{
			instruction = lType == Type::Int ? Instruction::Add : Instruction::FAdd;
			break;
		}
		case BinaryType::CompoundSub:
			Assert(lFlags & VariableFlags_Mutable, "Cannot assign to an immutable entity");
		case BinaryType::Subtract:
		{
			instruction = lType == Type::Int ? Instruction::Sub : Instruction::FSub;
			break;
		}
		case BinaryType::CompoundMul:
			Assert(lFlags & VariableFlags_Mutable, "Cannot assign to an immutable entity");
		case BinaryType::Multiply:
		{
			instruction = lType == Type::Int ? Instruction::Mul : Instruction::FMul;
			break;
		}
		case BinaryType::CompoundDiv:
			Assert(lFlags & VariableFlags_Mutable, "Cannot assign to an immutable entity");
		case BinaryType::Divide:
		{
			if (lType == Type::Int)
				throw CompileError("Integer division not supported");
		
			instruction = Instruction::FDiv;
			break;
		}
		case BinaryType::Assign:
		{
			Assert(lFlags & VariableFlags_Mutable, "Cannot assign to an immutable entity");

			return builder->CreateStore(right, left);
		}
		case BinaryType::Equal:
		{
			return lType == Type::Int ? builder->CreateICmpEQ(left, right, "cmptmp") :
				builder->CreateFCmpUEQ(left, right, "cmptmp");
		}
		case BinaryType::Less:
		{
			return lType == Type::Int ? builder->CreateICmpULT(left, right, "cmptmp") :
				builder->CreateFCmpULT(left, right, "cmptmp");
		}
		case BinaryType::LessEqual:
		{
			return lType == Type::Int ? builder->CreateICmpULE(left, right, "cmptmp") :
				builder->CreateFCmpULE(left, right, "cmptmp");
		}
		case BinaryType::Greater:
		{
			return lType == Type::Int ? builder->CreateICmpUGT(left, right, "cmptmp") :
				builder->CreateFCmpUGT(left, right, "cmptmp");
		}
		case BinaryType::GreaterEqual:
		{
			return lType == Type::Int ? builder->CreateICmpUGE(left, right, "cmptmp") :
				builder->CreateFCmpUGE(left, right, "cmptmp");
		}
	}

	Assert(instruction != (Instruction::BinaryOps)-1, "Invalid binary operator");
	return builder->CreateBinOp(instruction, left, right);
}

llvm::Value* Binary::Generate()
{
	llvm::Value* lhs = left->Generate();
	llvm::Value* rhs = right->Generate();

	VariableFlags lFlags = VariableFlags_None, rFlags = VariableFlags_None;

	// Handle flags
	auto leftName = lhs->getName().str(), rightName = rhs->getName().str();
	if (lhs->hasName() && namedValues.count(leftName))
		lFlags = namedValues[lhs->getName().str()].flags;
	if (rhs->hasName() && namedValues.count(rightName))
		rFlags = namedValues[rhs->getName().str()].flags;

	if (!left || !right)
		throw CompileError("Invalid binary operator '%.*s'", operatorToken.length, operatorToken.start); // What happon

	if (right->type != left->type)
		throw CompileError("Both operands of a binary operation must be of the same type");

	if (right->type == (Type)0 || left->type == (Type)0)
		throw CompileError("Invalid operands for binary operation");

	Type lType = left->type;
	
	llvm::Value* value = CreateBinOp(lhs, rhs, binaryType, lType, lFlags, rFlags);
	if (!value)
		throw CompileError("Invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

	return value;
}

llvm::Value* Branch::Generate()
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

static const char* LLVMTypeStringFromType(Type type)
{
	switch (type)
	{
	case Type::Int:     return "i32";
	case Type::Float:   return "float";
	case Type::Boolean: return "i1";
	default:
		throw CompileError("Invalid function call argument type");
	}
}

static void MangleFunctionName(std::string& name, const std::vector<FunctionDefinition::Parameter>& params)
{
	for (auto& param : params)
		name += LLVMTypeStringFromType(param.type);
}

llvm::Value* Call::Generate()
{
	// Mangle the function call name
	for (auto& arg : args)
	{
		fnName += LLVMTypeStringFromType(arg->type);
	}

	// Look up function name
	llvm::Function* func = module->getFunction(fnName);
	if (!func)
		throw CompileError("Unknown function referenced");

	// Handle arg mismatch
	if (func->arg_size() != args.size())
		throw CompileError("Incorrect number of arguments passed to '%s'", fnName.c_str());

	// Generate arguments
	std::vector<llvm::Value*> argValues;
	for (auto& expression : args)
	{
		llvm::Value* generated = (llvm::Value*)expression->Generate();
		if (!generated)
			throw CompileError("Failed to generate function argument");
		
		argValues.push_back(generated);
	}

	llvm::CallInst* callInst = builder->CreateCall(func, argValues);
	return callInst;
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

		std::string name = arg.getName().str();
		name.pop_back(); // Remove arg suffix

		AllocaInst* allocaInst = builder->CreateAlloca(type, nullptr, name);
		namedValues[name] = { allocaInst, VariableFlags_None };
	}
}

llvm::Value* FunctionDefinition::Generate()
{
	MangleFunctionName(name, params);

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
		printf("\nIR so far:\n");
		module->print(llvm::errs(), nullptr);
		printf("\n");

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

void Generator::Generate(std::unique_ptr<Compound> compound)
{
	try
	{
		for (auto& child : compound->statements)
		{
			child->Generate();
		}

		module->print(llvm::errs(), nullptr);
	}
	catch (CompileError& err)
	{
		fprintf(stderr, "CodeGenError: %s\n\n", err.message.c_str());

		printf("IR generated:\n");
		module->print(llvm::errs(), nullptr);
		printf("\n");
	}
}
