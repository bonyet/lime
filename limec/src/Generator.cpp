#include "limecpch.h"

#include "Tree.h"
#include "Generator.h"
#include "Casts.h"

#include "PlatformUtils.h"

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::Function* currentFunction = nullptr;

#define Assert(cond, line, msg, ...) { if (!(cond)) { throw CompileError(line, msg, __VA_ARGS__); } }
#define SoftAssert(cond, line, msg, ...) { if (!(cond)) { Warn(line, msg, __VA_ARGS__); } }

template<typename... Args>
static void Warn(int line, const std::string& message, Args&&... args)
{
	SetConsoleColor(14);
	fprintf(stdout, "warning (line %d): %s\n", line, FormatString(message.c_str(), std::forward<Args>(args)...).c_str());
	ResetConsoleColor();
}

#pragma region Casts

static llvm::Value* Int32ToInt64(llvm::Value* int32Value)
{
	return builder->CreateSExt(int32Value, Type::int64Type->raw, "sexttmp");
}
static llvm::Value* Int64ToInt32(llvm::Value* int64Value)
{
	return builder->CreateTrunc(int64Value, Type::int32Type->raw, "trunctmp");
}

#pragma endregion

std::vector<Cast> Cast::allowedImplicitCasts;

struct NamedValue
{
	llvm::Value* raw = nullptr;
	llvm::Type* type = nullptr;
	VariableDefinition::Modifiers modifiers;
};

static std::unordered_map<std::string, NamedValue> namedValues;
static std::unordered_map<llvm::StructType*, std::vector<std::string>> structureTypeMemberNames;

// Removes everything from namedValues that is not a global variable
static void ResetStackValues()
{
	PROFILE_FUNCTION();

	for (auto it = namedValues.begin(); it != namedValues.end(); )
	{
		if (!it->second.modifiers.isGlobal)
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

static bool IsPointer(const llvm::Type* const type) { return type->isPointerTy(); }
static bool IsSinglePointer(const llvm::Type* const type) { return type->getNumContainedTypes() == 1; }

static bool IsNumeric(llvm::Type* type)
{
	llvm::Type* actualType = type;
	if (actualType == llvm::Type::getInt1Ty(*context))
		return false; // Bool isn't numeric

	// Return value types
	if (actualType->isFunctionTy())
		actualType = ((llvm::FunctionType*)type)->getReturnType();

	return actualType->isIntegerTy() ||
		actualType->isFloatingPointTy();
}

llvm::Value* PrimaryValue::Generate()
{
	if (type->isInt32())
		return llvm::ConstantInt::get(*context, llvm::APInt(32, (int32_t)value.i64));
	if (type->isInt64())
		return llvm::ConstantInt::get(*context, llvm::APInt(64, value.i64));
	if (type->isFloat())
		return llvm::ConstantFP::get(*context, llvm::APFloat(value.f32));
	if (type->isBool())
		return llvm::ConstantInt::getBool(*context, value.b32);

	throw CompileError(line, "invalid type for primary value");
}

llvm::Value* StringValue::Generate()
{
	return builder->CreateGlobalStringPtr(value, "strtmp", 0U, module.get());
}

llvm::Value* VariableDefinition::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	Assert(!namedValues.count(name), line, "variable '%s' already defined", name.c_str());
	Assert(type->raw, line, "unresolved type for variable '%s'", name.c_str());

	bool isPointer = type->raw->isPointerTy();

	if (scope == 0)
	{
		// Global varable
		module->getOrInsertGlobal(name, type->raw);
		GlobalVariable* gVar = module->getNamedGlobal(name);
		gVar->setLinkage(GlobalValue::CommonLinkage);

		if (initializer)
			gVar->setInitializer(cast<Constant>(initializer->Generate()));

		namedValues[name] = { gVar, type->raw, modifiers };

		return gVar;
	}

	// Allocate on the stack
	AllocaInst* allocaInst = builder->CreateAlloca(type->raw, nullptr, name);
	namedValues[name] = { allocaInst, type->raw, modifiers };

	// Store initializer value into this variable if we have one
	if (initializer)
	{
		if (isPointer)
		{
			// stay safe kids
			if (initializer->type->raw != type->raw)
				throw CompileError(line, "address types do not match");

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

	Assert(namedValues.count(name), line, "unknown variable '%s'", name.c_str());

	NamedValue& value = namedValues[name];

	return emitInstruction ? builder->CreateLoad(value.type, value.raw, "loadtmp") : value.raw;
}

llvm::Value* Store::Generate()
{
	using namespace llvm;
	
	PROFILE_FUNCTION();

	// TODO: enforce shadowing rules

	Assert(namedValues.count(name), line, "unknown variable '%s'", name.c_str());

	NamedValue& namedValue = namedValues[name];
	Assert(!namedValue.modifiers.isConst, line, "cannot assign to an immutable variable");
	
	Value* value = right->Generate();
	if (value->getType() != namedValue.type)
	{
		// Try implicit cast
		llvm::Value* castAttempt = Cast::TryIfValid(value->getType(), namedValue.type, value);
		Assert(castAttempt, line, "assignment: illegal implicit cast to '%s'", type->name.c_str());
		value = castAttempt;
	}

	Assert(!IsPointer(namedValue.type), line,
		"operands for an assignment must be of the same type. got: %s = %s",
		type->name.c_str(), right->type->name.c_str());

	if (storeIntoLoad)
	{
		LoadInst* load = builder->CreateLoad(namedValue.raw, "loadtmp");
		return builder->CreateStore(value, load);
	}

	return builder->CreateStore(value, namedValue.raw);
}

static llvm::Value* GetOneNumericConstant(const Type* const type)
{
	if (type->isFloat())
		return llvm::ConstantFP::get(*context, llvm::APFloat(1.0f));
	else if (type->isInt())
		return llvm::ConstantInt::get(*context, llvm::APInt(32, 1, true));

	Assert(false, -1, "invalid type for GetOne(type)");
}

llvm::Value* Unary::Generate()
{
	using namespace llvm;

	PROFILE_FUNCTION();

	Value* value = operand->Generate();
	Value* loadedValue = nullptr;
	if (value->getType()->isPointerTy())
		loadedValue = builder->CreateLoad(value, "loadtmp");

	// TODO: generate the other unary operators
	// TODO: pointer arithmetic

	switch (unaryType)
	{
	case UnaryType::Not: // !value
	{
		if (!operand->type->isBool() && !operand->type->isInt())
			throw CompileError(line, "invalid operand for unary not (!). operand must be integral.");

		return builder->CreateNot(loadedValue ? loadedValue : value, "nottmp");
	}
	case UnaryType::Negate: // -value
	{
		if (!IsNumeric(operand->type->raw))
			throw CompileError(line, "invalid operand for unary negate (-). operand must be numerical.");

		if (operand->type->isFloat()) return builder->CreateFNeg(loadedValue ? loadedValue : value, "negtmp");
		if (operand->type->isInt())   return builder->CreateNeg(loadedValue ? loadedValue : value, "negtmp");

		break;
	}
	case UnaryType::PrefixIncrement:
	{
		if (!IsNumeric(operand->type->raw))
			throw CompileError(line, "invalid operand for unary increment (++). operand must be numerical.");

		builder->CreateStore(builder->CreateAdd(loadedValue, GetOneNumericConstant(operand->type), "inctmp"), value);
		return builder->CreateLoad(value, "loadtmp");
	}
	case UnaryType::PostfixIncrement:
	{
		if (!IsNumeric(operand->type->raw))
			throw CompileError(line, "invalid operand for unary increment (++). operand must be numerical.");

		llvm::Value* previousValue = builder->CreateAlloca(operand->type->raw, nullptr, "prevtmp");
		builder->CreateStore(loadedValue, previousValue); // Store previous value
		llvm::Value* result = builder->CreateAdd(loadedValue, GetOneNumericConstant(operand->type), "inctmp");
		builder->CreateStore(result, value);

		return builder->CreateLoad(previousValue, "prevload");
	}
	case UnaryType::PrefixDecrement:
	{
		if (!IsNumeric(operand->type->raw))
			throw CompileError(line, "invalid operand for unary decrement (--). operand must be numerical.");

		return builder->CreateStore(builder->CreateSub(loadedValue, GetOneNumericConstant(operand->type), "dectmp"), value);
	}
	case UnaryType::PostfixDecrement:
	{
		if (!IsNumeric(operand->type->raw))
			throw CompileError(line, "invalid operand for unary decrement (--). operand must be numerical.");
		
		llvm::Value* previousValue = builder->CreateStore(loadedValue, builder->CreateAlloca(operand->type->raw, nullptr, "prevtmp"));
		llvm::Value* result = builder->CreateAdd(loadedValue, GetOneNumericConstant(operand->type), "dectmp");
		builder->CreateStore(result, value);

		return previousValue;
	}
	case UnaryType::AddressOf:
		return value;
	case UnaryType::Deref:
		return loadedValue;
	}

	throw CompileError(line, "invalid unary operator");
}

static llvm::Value* CreateBinOp(llvm::Value* left, llvm::Value* right, BinaryType type, 
	const VariableDefinition::Modifiers* lhsMods, const VariableDefinition::Modifiers* rhsMods, int line, bool typeCheck = true)
{
	using llvm::Instruction;

	PROFILE_FUNCTION();

	llvm::Type* lType = left->getType();
	Instruction::BinaryOps instruction = (Instruction::BinaryOps)-1;

	if (typeCheck && lType != right->getType())
	{
		llvm::Value* castAttempt = Cast::TryIfValid(right->getType(), left->getType(), right);
		if (castAttempt) // Implicit cast worked
			Warn(line, "binary op: implicit cast from right operand type to left operand type");
		Assert(castAttempt, line, "binary op: illegal implicit cast from right operand type to left operand type");
		right = castAttempt;
	}

	// TODO: clean up
	switch (type)
	{
		case BinaryType::CompoundAdd:
			Assert(!lhsMods->isConst, line, "cannot assign to an immutable variable");
		case BinaryType::Add:
		{
			instruction = lType->isIntegerTy() ? Instruction::Add : Instruction::FAdd;
			break;
		}
		case BinaryType::CompoundSub:
			Assert(!lhsMods->isConst, line, "cannot assign to an immutable variable");
		case BinaryType::Subtract:
		{
			instruction = lType->isIntegerTy() ? Instruction::Sub : Instruction::FSub;
			break;
		}
		case BinaryType::CompoundMul:
			Assert(!lhsMods->isConst, line, "cannot assign to an immutable variable");
		case BinaryType::Multiply:
		{
			instruction = lType->isIntegerTy() ? Instruction::Mul : Instruction::FMul;
			break;
		}
		case BinaryType::CompoundDiv:
			Assert(!lhsMods->isConst, line, "cannot assign to an immutable variable");
		case BinaryType::Divide:
		{
			if (lType->isIntegerTy())
				throw CompileError(line, "integer division not supported");
		
			instruction = Instruction::FDiv;
			break;
		}
		case BinaryType::Assign:
		{
			Assert(!lhsMods->isConst, line, "cannot assign to an immutable variable");

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

	Assert(instruction != (Instruction::BinaryOps)-1, line, "invalid binary operator");
	return builder->CreateBinOp(instruction, left, right);
}

static VariableDefinition::Modifiers* TryGetVariableModifiers(Statement* statement)
{
	// TODO: ugly as hell
	if (statement->statementType == StatementType::LoadExpr)
		return &namedValues[((Load*)statement)->name].modifiers;
	if (statement->statementType == StatementType::StoreExpr)
		return &namedValues[((Store*)statement)->name].modifiers;
	if (statement->statementType == StatementType::UnaryExpr)
	{
		Unary* unary = (Unary*)statement;
		if (unary->unaryType == UnaryType::Deref)
			return TryGetVariableModifiers(unary->operand.get());
	}

	return nullptr;
}

llvm::Value* Binary::Generate()
{
	PROFILE_FUNCTION();

	llvm::Value* lhs = left->Generate();
	llvm::Value* rhs = right->Generate();

	// Uh what?
	Assert(left && right, line, "invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

	VariableDefinition::Modifiers* lMods = TryGetVariableModifiers(left.get());
	VariableDefinition::Modifiers* rMods = TryGetVariableModifiers(right.get());

	if (right->type != left->type)
	{
		bool valid = false;

		if (left->statementType == StatementType::UnaryExpr &&
			(left->type->isPointerTo(right->type) ||
				Cast::IsValid(right->type, left->type->getTypePointedTo()))) // Implicit cast to derefed pointer type
		{
			// Assigning a value to a dereference expression

			Unary* unary = static_cast<Unary*>(left.get());
			if (unary->unaryType == UnaryType::Deref && binaryType == BinaryType::Assign)
			{
				// Looks good 2 me
				valid = true;
			}

			// Implicitly cast?
			llvm::Value* castAttempt = Cast::TryIfValid(right->type, left->type->getTypePointedTo(), rhs);
			if (castAttempt) // Implicit cast worked
				Warn(line, "binary op: implicit cast from right operand type to left operand type");
			Assert(castAttempt, line, "binary op: illegal implicit cast from right operand type to left operand type");
			rhs = castAttempt;
		}
		else
		{
			// Implicitly cast?
			if (Cast::IsValid(right->type, left->type))
				valid = true;
		}

		
		Assert(valid, line, "both operands of a binary operation must be of the same type");
	}

	Assert(right->type && left->type, line, "invalid operands for binary operation");
	
	llvm::Value* value = CreateBinOp(lhs, rhs, binaryType, lMods, rMods, line, false);
	Assert(value, line, "invalid binary operator '%.*s'", operatorToken.length, operatorToken.start);

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
	BranchInst* branch = builder->CreateCondBr(expression->Generate(), trueBlock, falseBlock);

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
	Assert(func, line, "unknown function referenced");

	// Handle arg mismatch
	bool isVarArg = func->isVarArg();
	if (isVarArg)
	{
		Assert(args.size() >= func->arg_size() - 1, line, "not enough arguments passed to '%s'", fnName.c_str());
	}
	else
	{
		Assert(args.size() == func->arg_size(), line, "incorrect number of arguments passed to '%s'", fnName.c_str());
	}

	// Generate arguments
	int i = 0;
	std::vector<llvm::Value*> argValues;
	for (auto& expression : args)
	{
		llvm::Value* generated = expression->Generate();
		Assert(generated, line, "failed to generate function argument");

		if (generated->getType() != func->getArg(i)->getType())
		{
			// Try an implicit cast
			llvm::Value* castAttempt = Cast::TryIfValid(generated->getType(), func->getArg(i)->getType(), generated);
			if (castAttempt)
				Warn(line, 
				"call: implicit cast from argument %d (type of '%s') to '%s' parameter %d (type of '%s')", 
				i, args[i]->type->name.c_str(), target->name.c_str(), i, 
				target->params[i].variadic ? "..." : target->params[i].type->name.c_str());
			Assert(castAttempt, line,
					"illegal call: implicit cast from argument %d (type of '%s') to '%s' parameter %d (type of '%s') not allowed",
					i, args[i]->type->name.c_str(), target->name.c_str(), i,
					target->params[i].variadic ? "..." : target->params[i].type->name.c_str());
			generated = castAttempt;
		}
		
		++i;
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

		namedValues[name] = { allocaInst, type, { } };
	}
}

llvm::Value* Return::Generate()
{
	PROFILE_FUNCTION();

	// Handle the return value
	llvm::Value* returnValue = expression->Generate();
	llvm::Value* result = returnValue;
	if (returnValue->getType() != currentFunction->getReturnType())
	{
		result = Cast::TryIfValid(returnValue->getType(), currentFunction->getReturnType(), returnValue);
		if (result)
			Warn(line, "return statement for function '%s': implicit cast to return type from '%s'", currentFunction->getName().data(), expression->type->name.c_str());
		Assert(result, line, "invalid return value for function '%s': illegal implicit cast to return type from '%s'", currentFunction->getName().data(), expression->type->name.c_str());
	}
	
	return builder->CreateRet(result);
}

llvm::Value* Import::Generate()
{
	return data->Generate();
}

static llvm::Value* GenerateFunctionPrototype(const FunctionDefinition* definition, const FunctionPrototype& prototype)
{
	std::vector<llvm::Type*> paramTypes(prototype.params.size());

	// Fill paramTypes with proper types
	int index = 0;
	for (auto& param : prototype.params)
	{
		if (param.variadic)
			paramTypes[index++] = llvm::Type::getInt32Ty(*context);
		else
			paramTypes[index++] = param.type->raw;
	}
	index = 0;

	bool hasVarArg = false;
	for (int i = 0; i < prototype.params.size(); i++)
	{
		if (prototype.params[i].variadic)
			hasVarArg = true;
	}

	llvm::Type* returnType = definition->prototype.returnType->raw;

	Assert(returnType, definition->line, "unresolved return type for function prototype '%s'", prototype.name.c_str());

	llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, hasVarArg);
	llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, prototype.name.c_str(), *module);

	// Set names for args
	for (auto& arg : function->args())
		arg.setName(std::to_string(index++));

	return function;
}

llvm::Value* FunctionDefinition::Generate()
{
	PROFILE_FUNCTION();

	if (!HasBody()) // We are just a prototype (probably in an "import" statement or something)
		return GenerateFunctionPrototype(this, prototype);

	// Full definition for function
	llvm::Function* function = module->getFunction(prototype.name.c_str());
	// Create function if it doesn't exist
	if (!function)
	{
		std::vector<llvm::Type*> paramTypes(prototype.params.size());

		// Fill paramTypes with proper types
		int index = 0;
		for (auto& param : prototype.params)
			paramTypes[index++] = param.type->raw;
		index = 0;

		llvm::Type* returnType = prototype.returnType->raw;
		Assert(returnType, line, "unresolved return type for '%s'", prototype.name.c_str());

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, prototype.name.c_str(), *module);

		// Set names for function args
		for (auto& arg : function->args())
			arg.setName(prototype.params[index++].name + '_'); // Suffix arguments with '_' to make stuff work
	}

	Assert(function->empty(), line, "function cannot be redefined");

	currentFunction = function;

	// Create block to start insertion into
	llvm::BasicBlock* block = llvm::BasicBlock::Create(*context, "entry", function);
	builder->SetInsertPoint(block);

	// Allocate args on the stack
	ResetStackValues();
	GenerateEntryBlockAllocasAndLoads(function);

	{
		PROFILE_SCOPE("Generate body :: FunctionDefinition::Generate()");
		
		// Generate body
		for (auto& statement : body)
			statement->Generate();
	}

	if (!block->getTerminator())
	{
		Assert(type->isVoid(), line, "return statement not found in function '%s'", prototype.name.c_str());

		builder->CreateRet(nullptr);
	}

	{
		PROFILE_SCOPE("Verify function :: FunctionDefinition::Generate()");

		// Handle any errors in the function
		if (verifyFunction(*function, &llvm::errs()))
		{
			//module->print(llvm::errs(), nullptr);
			function->eraseFromParent();
			throw CompileError(line, "function verification failed");
		}
	}

	return function;
}

llvm::Value* StructureDefinition::Generate()
{
	PROFILE_FUNCTION();
	
	Assert(members.size(), line, "structs must own at least one member");

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

	// Resolve casts
	Cast::allowedImplicitCasts =
	{
		Cast(Type::int32Type, Type::int64Type, Int32ToInt64, true),
		Cast(Type::int64Type, Type::int32Type, Int64ToInt32, true),
	};
	
	// Resolve the primitive types
	{
		Type::int8Type->raw   = llvm::Type::getInt8Ty(*context);
		Type::int32Type->raw  = llvm::Type::getInt32Ty(*context);
		Type::int64Type->raw  = llvm::Type::getInt64Ty(*context);
		Type::floatType->raw  = llvm::Type::getFloatTy(*context);
		Type::boolType->raw   = llvm::Type::getInt1Ty(*context);
		Type::stringType->raw = llvm::Type::getInt8PtrTy(*context);
		Type::voidType->raw   = llvm::Type::getVoidTy(*context);

		Type::int8PtrType->raw   = llvm::Type::getInt8PtrTy(*context);
		Type::int32PtrType->raw  = llvm::Type::getInt32PtrTy(*context);
		Type::int64PtrType->raw  = llvm::Type::getInt64PtrTy(*context);
		Type::floatPtrType->raw  = llvm::Type::getFloatPtrTy(*context);
		Type::boolPtrType->raw   = llvm::Type::getInt1PtrTy(*context);
		Type::stringPtrType->raw = llvm::Type::getInt8PtrTy(*context)->getPointerTo();
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
		{
			child->Generate();
		}

		llvm::raw_string_ostream stream(result.ir);
		module->print(stream, nullptr);
		result.Succeeded = true;
	}
	catch (const CompileError& err)
	{
		result.Succeeded = false;
		SetConsoleColor(12);
		fprintf(stderr, "error (line %d): %s\n", err.line, err.message.c_str());
		ResetConsoleColor();
	}

	Typer::Release();

	return result;
}