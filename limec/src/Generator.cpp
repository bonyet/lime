#include <string>
#include "Utils.h"

#include "Error.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Value.h>

#include "Tree.h"
#include "Typer.h"
#include "Generator.h"
#include "Profiler.h"

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::Function* currentFunction = nullptr;

#define Assert(cond, msg) { if (!(cond)) { throw CompileError(msg); } }

struct NamedValue
{
	llvm::Value* raw = nullptr;
	llvm::Type* type = nullptr;
	VariableFlags flags = VariableFlags_None;
};

static std::unordered_map<std::string, NamedValue> namedValues;

static std::unordered_map<llvm::StructType*, std::vector<std::string>> structureTypeMemberNames;

llvm::Value* PrimaryValue::Generate()
{
	switch (type->name[0])
	{
	case 'i':
		return llvm::ConstantInt::get(*context, llvm::APInt(32, value.i32));
	case 'f':
		return llvm::ConstantFP::get(*context, llvm::APFloat(value.f32));
	case 'b':
		return llvm::ConstantInt::getBool(*context, value.b32);
	}
}

static void ResetStackValues()
{
	PROFILE_FUNCTION();
	
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
	
	PROFILE_FUNCTION();
	
	if (namedValues.count(name))
	{
		throw CompileError("variable '%s' already defined", name.c_str());
	}

	llvm::Type* type = this->type->raw;
	if (scope == 0)
	{
		// Global varable
		module->getOrInsertGlobal(name, type);
		GlobalVariable* gVar = module->getNamedGlobal(name);
		gVar->setLinkage(GlobalValue::CommonLinkage);

		if (initializer)
			gVar->setInitializer(static_cast<Constant*>(initializer->Generate()));

		namedValues[name] = { gVar, type, flags };

		return gVar;
	}

	// Allocate on the stack
	AllocaInst* allocaInst = builder->CreateAlloca(type, nullptr, name);
	namedValues[name] = { allocaInst, type, flags };

	// Store initializer value into this variable if we have one
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
	
	PROFILE_FUNCTION();

	if (!namedValues.count(name))
		throw CompileError("unknown variable '%s'", name.c_str());

	NamedValue& value = namedValues[name];
	return builder->CreateLoad(value.raw, "loadtmp");
}

llvm::Value* VariableWrite::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	// TODO: enforce shadowing rules

	if (!namedValues.count(name))
		throw CompileError("unknown variable '%s'", name.c_str());

	NamedValue& namedValue = namedValues[name];
	
	Value* value = right->Generate();
	return builder->CreateStore(value, namedValue.raw, "storetmp");
}

static llvm::Value* CreateBinOp(llvm::Value* left, llvm::Value* right, 
	BinaryType type, Type* lType, VariableFlags lFlags, VariableFlags rFlags)
{
	using llvm::Instruction;

	PROFILE_FUNCTION();

	Instruction::BinaryOps instruction = (Instruction::BinaryOps)-1;
	switch (type)
	{
		case BinaryType::CompoundAdd:
			Assert(lFlags & VariableFlags_Mutable, "cannot assign to an immutable entity");
		case BinaryType::Add:
		{
			instruction = lType->isInt() ? Instruction::Add : Instruction::FAdd;
			break;
		}
		case BinaryType::CompoundSub:
			Assert(lFlags & VariableFlags_Mutable, "cannot assign to an immutable entity");
		case BinaryType::Subtract:
		{
			instruction = lType->isInt() ? Instruction::Sub : Instruction::FSub;
			break;
		}
		case BinaryType::CompoundMul:
			Assert(lFlags & VariableFlags_Mutable, "cannot assign to an immutable entity");
		case BinaryType::Multiply:
		{
			instruction = lType->isInt() ? Instruction::Mul : Instruction::FMul;
			break;
		}
		case BinaryType::CompoundDiv:
			Assert(lFlags & VariableFlags_Mutable, "cannot assign to an immutable entity");
		case BinaryType::Divide:
		{
			if (lType->isInt())
				throw CompileError("integer division not supported");
		
			instruction = Instruction::FDiv;
			break;
		}
		case BinaryType::Assign:
		{
			Assert(lFlags & VariableFlags_Mutable, "cannot assign to an immutable entity");

			return builder->CreateStore(right, left);
		}
		case BinaryType::Equal:
		{
			return lType->isInt() ? builder->CreateICmpEQ(left, right, "cmptmp") :
				builder->CreateFCmpUEQ(left, right, "cmptmp");
		}
		case BinaryType::Less:
		{
			return lType->isInt() ? builder->CreateICmpULT(left, right, "cmptmp") :
				builder->CreateFCmpULT(left, right, "cmptmp");
		}
		case BinaryType::LessEqual:
		{
			return lType->isInt() ? builder->CreateICmpULE(left, right, "cmptmp") :
				builder->CreateFCmpULE(left, right, "cmptmp");
		}
		case BinaryType::Greater:
		{
			return lType->isInt() ? builder->CreateICmpUGT(left, right, "cmptmp") :
				builder->CreateFCmpUGT(left, right, "cmptmp");
		}
		case BinaryType::GreaterEqual:
		{
			return lType->isInt() ? builder->CreateICmpUGE(left, right, "cmptmp") :
				builder->CreateFCmpUGE(left, right, "cmptmp");
		}
	}

	Assert(instruction != (Instruction::BinaryOps)-1, "invalid binary operator");
	return builder->CreateBinOp(instruction, left, right);
}

llvm::Value* Binary::Generate()
{
	PROFILE_FUNCTION();
	
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
		throw CompileError("invalid binary operator '%.*s'", operatorToken.length, operatorToken.start); // What happon

	if (right->type != left->type)
		throw CompileError("both operands of a binary operation must be of the same type");

	if (!right->type || !left->type)
		throw CompileError("invalid operands for binary operation");

	Type* lType = left->type;
	
	llvm::Value* value = CreateBinOp(lhs, rhs, binaryType, lType, lFlags, rFlags);
	if (!value)
		throw CompileError("invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

	return value;
}

llvm::Value* Branch::Generate()
{
	using namespace llvm;

	PROFILE_FUNCTION();
	
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

llvm::Value* Call::Generate()
{
	PROFILE_FUNCTION();
	
	// Look up function name
	llvm::Function* func = module->getFunction(fnName);
	if (!func)
		throw CompileError("unknown function referenced");

	// Handle arg mismatch
	if (func->arg_size() != args.size())
		throw CompileError("incorrect number of arguments passed to '%s'", fnName.c_str());

	// Generate arguments
	std::vector<llvm::Value*> argValues;
	for (auto& expression : args)
	{
		llvm::Value* generated = (llvm::Value*)expression->Generate();
		if (!generated)
			throw CompileError("failed to generate function argument");
		
		argValues.push_back(generated);
	}

	llvm::CallInst* callInst = builder->CreateCall(func, argValues, "calltmp");
	return callInst;
}

static void GenerateEntryBlockAllocas(llvm::Function* function)
{
	using namespace llvm;

	PROFILE_FUNCTION();
	
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
		namedValues[name] = { allocaInst, type, VariableFlags_None };
	}
}

llvm::Value* FunctionDefinition::Generate()
{
	PROFILE_FUNCTION();
	 
	llvm::Function* function = module->getFunction(name.c_str());
	// Create function if it doesn't exist
	if (!function)
	{
		std::vector<llvm::Type*> paramTypes(params.size());

		// Fill paramTypes with proper types
		int index = 0;
		for (auto& param : params)
			paramTypes[index++] = param.type->raw;
		index = 0;

		llvm::Type* returnType = type->raw;

		if (!returnType)
			throw CompileError("invalid return type for '%s'", name.c_str());

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name.c_str(), *module);

		// Set names for function args
		for (auto& arg : function->args())
		{
			arg.setName(params[index++].name + '_'); // Suffix arguments with '_' to make stuff work
		}
	}

	if (!function->empty())
		throw CompileError("function cannot be redefined");

	currentFunction = function;

	// Create block to start insertion into
	llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(block);

	// Allocate args on the stack
	namedValues.clear();
	GenerateEntryBlockAllocas(function);

	{
		PROFILE_SCOPE("Generate body :: FunctionDefinition::Generate()");
		
		// Generate body
		int index = 0;
		for (auto& statement : body)
		{
			if (index++ == indexOfReturnInBody)
				continue; // Don't generate return statements twice

			statement->Generate();
		}
	}

	// Handle the return value
	llvm::Value* returnValue = indexOfReturnInBody != -1 ? (llvm::Value*)body[indexOfReturnInBody]->Generate() : nullptr;
	builder->CreateRet(returnValue);

	{
		PROFILE_SCOPE("Verify function :: FunctionDefinition::Generate()");

		if (verifyFunction(*function, &llvm::errs()))
		{
			printf("\nIR so far:\n");
			module->print(llvm::errs(), nullptr);
			printf("\n");

			function->eraseFromParent();
			printf("\n");
		}
	}

	return function;
}

llvm::Value* StructureDefinition::Generate()
{
	PROFILE_FUNCTION();
	
	Assert(members.size(), "structs must own at least one member");

	// Type already resolved

	return nullptr;
}

llvm::Value* MemberRead::Generate()
{
	PROFILE_FUNCTION();

	NamedValue& structure = namedValues[variableName];

	UserDefinedType* userStructureType = static_cast<UserDefinedType*>(Typer::Get(variableTypename));

	int structureIndex = -1;
	{
		int index = 0;
		for (auto& pair : userStructureType->members)
		{
			if (pair.first == memberName)
			{
				structureIndex = index;
				break;
			}

			index++;
		}
	}
	Assert(structureIndex >= 0, "invalid member for structure");

	std::array<llvm::Value*, 2> indices
	{
		llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)),
		llvm::ConstantInt::get(*context, llvm::APInt(32, structureIndex, true)),
	};

	llvm::Value* pRetrievedValue = builder->CreateGEP(type->raw, structure.raw, indices, "geptmp");


	// Modify
	return builder->CreateLoad(pRetrievedValue, "loadtmp");
}

llvm::Value* MemberWrite::Generate()
{
	PROFILE_FUNCTION();
	
	NamedValue& structure = namedValues[variableName];

	UserDefinedType* userStructureType = static_cast<UserDefinedType*>(Typer::Get(variableTypename));
	
	int structureIndex = -1;
	{
		int index = 0;
		for (auto& pair : userStructureType->members)
		{
			if (pair.first == memberName)
			{
				structureIndex = index;
				break;
			}

			index++;
		}
	}
	Assert(structureIndex >= 0, "invalid member for structure");

	std::array<llvm::Value*, 2> indices
	{
		llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)),
		llvm::ConstantInt::get(*context, llvm::APInt(32, structureIndex, true)),
	};

	llvm::Value* pRetrievedValue = builder->CreateGEP(type->raw, structure.raw, indices, "geptmp");

	// Modify
	return builder->CreateStore(right->Generate(), pRetrievedValue);
}

Generator::Generator()
{
	PROFILE_FUNCTION();
	
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>(llvm::StringRef(), *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

static void ResolveType(UserDefinedType* type)
{
	PROFILE_FUNCTION();
	
	std::vector<llvm::Type*> memberTypesForLLVM;
	memberTypesForLLVM.resize(type->members.size());
	
	// TODO: non-primitive member types for structures
	int index = 0;
	for (auto& memberType : type->members)
		memberTypesForLLVM[index++] = memberType.second->raw;

	type->raw = llvm::StructType::create(*context, memberTypesForLLVM, type->name, false);
}

static void ResolveParsedTypes(ParseResult& result)
{
	PROFILE_FUNCTION();
	
	// Resolve the primitive types
	{
		Type::int32Type->raw  = llvm::Type::getInt32Ty(*context);
		Type::floatType->raw  = llvm::Type::getFloatTy(*context);
		Type::boolType->raw   = llvm::Type::getInt1Ty(*context);
		Type::stringType->raw = llvm::Type::getInt1PtrTy(*context);
		Type::voidType->raw   = llvm::Type::getVoidTy(*context);
	}

	// Resolve non-primitives
	for (Type* type : Typer::GetAll())
	{
		if (!type->isPrimitive())
		{
			ResolveType(static_cast<UserDefinedType*>(type));
		}
	}
}

CompileResult Generator::Generate(ParseResult& parseResult)
{
	PROFILE_FUNCTION();
	
	CompileResult result;

	try
	{
		ResolveParsedTypes(parseResult);

		for (auto& child : parseResult.module->statements)
		{
			child->Generate();
		}

		llvm::raw_string_ostream stream(result.ir);
		module->print(stream, nullptr);
		result.Succeeded = true;
	}
	catch (CompileError& err)
	{
		result.Succeeded = false;

		fprintf(stderr, "CodeGenError: %s\n\n", err.message.c_str());
	}

	return result;
}
