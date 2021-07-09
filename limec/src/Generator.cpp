#include "limecpch.h"

#include "Tree.h"
#include "Typer.h"
#include "Generator.h"

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::Function* currentFunction = nullptr;

#define Assert(cond, msg, ...) { if (!(cond)) { throw CompileError(msg, __VA_ARGS__); } }

struct NamedValue
{
	llvm::Value* raw = nullptr;
	llvm::Type* type = nullptr;
	VariableFlags flags = VariableFlags_None;
};

static std::unordered_map<std::string, NamedValue> namedValues;

static std::unordered_map<llvm::StructType*, std::vector<std::string>> structureTypeMemberNames;

// Removes everything from namedValues that is not a global variable
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

static bool IsUnaryDereference(const std::unique_ptr<Expression>& expr)
{
	bool isUnary = expr->statementType == StatementType::UnaryExpr;
	if (!isUnary)
		return false;

	return static_cast<Unary*>(expr.get())->unaryType == UnaryType::Deref;
}

static bool IsPointer(const Load* const variable) { return variable->type->isPointer(); }

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

	throw CompileError("invalid type for primary value");
}

llvm::Value* VariableDefinition::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	Assert(!namedValues.count(name), "variable '%s' already defined", name.c_str());

	// TODO: deduce type from function calls (just use return type)
	Assert(type->raw, "unresolved type for variable '%s'", name.c_str());

	bool isPointer = type->raw->isPointerTy();

	if (scope == 0)
	{
		// Global varable
		module->getOrInsertGlobal(name, type->raw);
		GlobalVariable* gVar = module->getNamedGlobal(name);
		gVar->setLinkage(GlobalValue::CommonLinkage);

		if (initializer)
			gVar->setInitializer(cast<Constant>(initializer->Generate()));

		namedValues[name] = { gVar, type->raw, flags };

		return gVar;
	}

	// Allocate on the stack
	AllocaInst* allocaInst = builder->CreateAlloca(type->raw, nullptr, name);
	namedValues[name] = { allocaInst, type->raw, flags };

	// Store initializer value into this variable if we have one
	if (initializer)
	{
		if (isPointer)
		{
			// type safety
			if (initializer->type->raw != type->raw)
				throw CompileError("address types do not match");

			Value* value = initializer->Generate();

			builder->CreateStore(value, allocaInst);
		}
		else
		{
			Value* value = initializer->Generate();

			builder->CreateStore(value, allocaInst);
		}
	}

	return allocaInst;
}

llvm::Value* Load::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	Assert(namedValues.count(name), "unknown variable '%s'", name.c_str());

	NamedValue& value = namedValues[name];

	return builder->CreateLoad(value.type, value.raw, "loadtmp");
}

llvm::Value* Store::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	// TODO: enforce shadowing rules

	Assert(namedValues.count(name), "unknown variable '%s'", name.c_str());

	NamedValue& namedValue = namedValues[name];
	
	Value* value = right->Generate();

	if (storeIntoLoad)
	{
		LoadInst* load = builder->CreateLoad(namedValue.raw, "loadtmp");
		return builder->CreateStore(value, load);
	}

	return builder->CreateStore(value, namedValue.raw);
}

llvm::Value* Unary::Generate()
{
	using namespace llvm;

	PROFILE_FUNCTION();

	Value* value = operand->Generate();

	// TODO: generate the other unary operators

	switch (unaryType)
	{
	case UnaryType::Not: // !value
	{
		if (!operand->type->isBool() && !operand->type->isInt())
			throw CompileError("invalid operand for unary not (!). operand must be integral.");

		return builder->CreateNot(value, "nottmp");
	}
	case UnaryType::Negate: // -value
	{
		if (!operand->type->isInt())
			throw CompileError("invalid operand for unary negate (-). operand must be integral.");

		return builder->CreateNeg(value, "negtmp");
	}
	case UnaryType::AddressOf:
		return value; // Created via an alloca
	case UnaryType::Deref:
		return builder->CreateLoad(value, "dereftmp");
	}

	throw CompileError("invalid unary operator");
}

static llvm::Value* CreateBinOp(llvm::Value* left, llvm::Value* right, 
	BinaryType type, VariableFlags lFlags, VariableFlags rFlags)
{
	using llvm::Instruction;

	PROFILE_FUNCTION();

	llvm::Type* lType = left->getType();
	Instruction::BinaryOps instruction = (Instruction::BinaryOps)-1;

	// TODO: clean up
	switch (type)
	{
		case BinaryType::CompoundAdd:
			Assert(!(lFlags & VariableFlags_Immutable), "cannot assign to an immutable entity");
		case BinaryType::Add:
		{
			instruction = lType->isIntegerTy() ? Instruction::Add : Instruction::FAdd;
			break;
		}
		case BinaryType::CompoundSub:
			Assert(!(lFlags & VariableFlags_Immutable), "cannot assign to an immutable entity");
		case BinaryType::Subtract:
		{
			instruction = lType->isIntegerTy() ? Instruction::Sub : Instruction::FSub;
			break;
		}
		case BinaryType::CompoundMul:
			Assert(!(lFlags & VariableFlags_Immutable), "cannot assign to an immutable entity");
		case BinaryType::Multiply:
		{
			instruction = lType->isIntegerTy() ? Instruction::Mul : Instruction::FMul;
			break;
		}
		case BinaryType::CompoundDiv:
			Assert(!(lFlags & VariableFlags_Immutable), "cannot assign to an immutable entity");
		case BinaryType::Divide:
		{
			if (lType->isIntegerTy())
				throw CompileError("integer division not supported");
		
			instruction = Instruction::FDiv;
			break;
		}
		case BinaryType::Assign:
		{
			Assert(!(lFlags & VariableFlags_Immutable), "cannot assign to an immutable entity");

			return builder->CreateStore(right, left);
		}
		case BinaryType::Equal:
		{
			if (lType->isIntegerTy())
				return builder->CreateICmpEQ(left, right, "cmptmp");
			else if (lType->isFloatingPointTy())
				return builder->CreateFCmpUEQ(left, right, "cmptmp");
			break;
		}
		case BinaryType::NotEqual:
		{
			if (lType->isIntegerTy())
				return builder->CreateICmpNE(left, right, "cmptmp");
			else if (lType->isFloatingPointTy())
				return builder->CreateFCmpUNE(left, right, "cmptmp");
			break;
		}
		case BinaryType::Less:
		{
			return lType->isIntegerTy() ? builder->CreateICmpULT(left, right, "cmptmp") :
				builder->CreateFCmpULT(left, right, "cmptmp");
		}
		case BinaryType::LessEqual:
		{
			return lType->isIntegerTy() ? builder->CreateICmpULE(left, right, "cmptmp") :
				builder->CreateFCmpULE(left, right, "cmptmp");
		}
		case BinaryType::Greater:
		{
			return lType->isIntegerTy() ? builder->CreateICmpUGT(left, right, "cmptmp") :
				builder->CreateFCmpUGT(left, right, "cmptmp");
		}
		case BinaryType::GreaterEqual:
		{
			return lType->isIntegerTy() ? builder->CreateICmpUGE(left, right, "cmptmp") :
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

	// Uh what?
	Assert(left && right, "invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

	// Handle flags
	{
		// TODO: remove this shit

		if (left->statementType == StatementType::VariableReadExpr)
			lFlags = namedValues[static_cast<Load*>(left.get())->name].flags;
		else if (left->statementType == StatementType::VariableWriteExpr)
			lFlags = namedValues[static_cast<Store*>(left.get())->name].flags;

		if (right->statementType == StatementType::VariableReadExpr)
			rFlags = namedValues[static_cast<Load*>(right.get())->name].flags;
		else if (right->statementType == StatementType::VariableWriteExpr)
			rFlags = namedValues[static_cast<Store*>(right.get())->name].flags;
	}

	Assert(right->type == left->type, "both operands of a binary operation must be of the same type");
	Assert(right->type && left->type, "invalid operands for binary operation");
	
	llvm::Value* value = CreateBinOp(lhs, rhs, binaryType, lFlags, rFlags);
	Assert(value, "invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

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
		auto createBlock = [endBlock](BasicBlock* block, const std::vector<std::unique_ptr<Statement>>& statements)
		{
			builder->SetInsertPoint(block);

			bool terminated = false;
			for (auto& statement : statements)
			{
				statement->Generate();

				if (statement->statementType == StatementType::ReturnExpr)
				{
					terminated = true;
					break;
				}
			}

			// Add jump to end
			if (!terminated)
				builder->CreateBr(endBlock);
		};

		// Generate body for the true branch
		createBlock(trueBlock, ifBody);

		// Generate body for the true branch
		createBlock(falseBlock, elseBody);
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
	Assert(func, "unknown function referenced");

	// Handle arg mismatch
	Assert(args.size() == func->arg_size(), "incorrect number of arguments passed to '%s'", fnName.c_str());

	// Generate arguments
	std::vector<llvm::Value*> argValues;
	for (auto& expression : args)
	{
		llvm::Value* generated = (llvm::Value*)expression->Generate();
		Assert(generated, "failed to generate function argument");
		
		argValues.push_back(generated);
	}

	llvm::CallInst* callInst = builder->CreateCall(func, argValues);
	return callInst;
}

static void GenerateEntryBlockAllocasAndLoads(llvm::Function* function)
{
	using namespace llvm;

	PROFILE_FUNCTION();
	
	for (Argument* arg = function->arg_begin(); arg != function->arg_end(); ++arg)
	{
		llvm::Type* type = arg->getType();

		std::string name = arg->getName().str();
		name.pop_back(); // Remove arg suffix

		AllocaInst* allocaInst = builder->CreateAlloca(type, nullptr, name);
		builder->CreateStore(arg, allocaInst); // Store argument value into allocated value

		namedValues[name] = { allocaInst, type, VariableFlags_None };
	}
}

llvm::Value* Return::Generate()
{
	PROFILE_FUNCTION();

	// Handle the return value
	llvm::Value* returnValue = expression->Generate();
	return builder->CreateRet(returnValue);
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

		Assert(returnType, "invalid return type for '%s'", name.c_str());

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name.c_str(), *module);

		// Set names for function args
		for (auto& arg : function->args())
			arg.setName(params[index++].name + '_'); // Suffix arguments with '_' to make stuff work
	}

	Assert(function->empty(), "function cannot be redefined");

	currentFunction = function;

	// Create block to start insertion into
	llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(block);

	// Allocate args on the stack
	namedValues.clear();
	GenerateEntryBlockAllocasAndLoads(function);

	{
		PROFILE_SCOPE("Generate body :: FunctionDefinition::Generate()");
		
		// Generate body
		for (auto& statement : body)
			statement->Generate();
	}

	if (!block->getTerminator())
	{
		Assert(type->isVoid(), "return statement not found in function '%s'", name.c_str());

		builder->CreateRet(nullptr);
	}

	{
		PROFILE_SCOPE("Verify function :: FunctionDefinition::Generate()");

		// Handle any errors in the function
		if (verifyFunction(*function, &llvm::errs()))
		{
			//module->print(llvm::errs(), nullptr);
			function->eraseFromParent();
			throw CompileError("function verification failed");
		}
	}

	return function;
}

llvm::Value* StructureDefinition::Generate()
{
	PROFILE_FUNCTION();
	
	Assert(members.size(), "structs must own at least one member");

	// The heavy lifting was done in ResolveType(), so we don't really do anything here

	return nullptr;
}

Generator::Generator()
{
	PROFILE_FUNCTION();
	
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>(llvm::StringRef(), *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);
}

static Type* FindCorrespondingPointerType(Type* type)
{
	std::string typeName = "*" + type->name;
	for (Type* other : Typer::GetAll())
	{
		if (other->name == typeName)
			return other;
	}

	return nullptr;
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

		Type::int32PtrType->raw  = llvm::Type::getInt32PtrTy(*context);
		Type::floatPtrType->raw  = llvm::Type::getFloatPtrTy(*context);
		Type::boolPtrType->raw   = llvm::Type::getInt1PtrTy(*context);
		Type::stringPtrType->raw = llvm::Type::getInt1PtrTy(*context);
	}

	// Resolve non primitive types here
}

CompileResult Generator::Generate(ParseResult& parseResult)
{
	PROFILE_FUNCTION();
	
	CompileResult result;

	try
	{
		ResolveParsedTypes(parseResult);

		// Generate all the statements & expressions
		for (auto& child : parseResult.module->statements)
			child->Generate();

		llvm::raw_string_ostream stream(result.ir);
		module->print(stream, nullptr);
		result.Succeeded = true;
	}
	catch (CompileError& err)
	{
		result.Succeeded = false;

		fprintf(stderr, "Code generation error: %s\n\n", err.message.c_str());
	}

	Typer::Release();

	return result;
}
