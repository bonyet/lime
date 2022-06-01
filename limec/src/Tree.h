#pragma once

#include "Lexer.h"
#include "Type.h"

enum class StatementType
{
	Default = 1,
	Compound, Import,
	PrimaryValue, StringValue, 
	UnaryExpr, BinaryExpr,
	Branch,
	CallExpr,
	ReturnExpr,
	FunctionDefine, FunctionPrototypeDefine,
	VariableDefine,
	StructureDefine,
	MemberLoadExpr, LoadExpr,
	MemberStoreExpr, StoreExpr,
};

struct Statement
{
	int line = 0;
	StatementType statementType = (StatementType)0;

	virtual ~Statement() {}

	virtual llvm::Value* Generate() { return nullptr; }
};

// Block of statements
struct Compound : public Statement
{
	std::vector<std::unique_ptr<Statement>> statements;

	Compound()
	{
		statementType = StatementType::Compound;
	}
};

struct Expression : public Statement
{
	Type* type = nullptr;
};

struct Primary : public Expression
{
	Token token;
};

// Stores int, float, or bool
struct PrimaryValue : public Primary
{
	union
	{
		int64_t i64 = 0;
		int64_t* ip64;
		bool b32;
		float f32;
	} value;

	PrimaryValue()
	{
		statementType = StatementType::PrimaryValue;
	}

	llvm::Value* Generate() override;
};

struct StringValue : public Primary
{
	std::string value;

	StringValue()
	{
		statementType = StatementType::StringValue;
	}
	
	llvm::Value* Generate() override;
};

enum class UnaryType
{
	Not = 1,
	Negate,
	PrefixIncrement, PrefixDecrement,
	PostfixIncrement, PostfixDecrement,

	AddressOf, Deref,
};

struct Unary : public Expression
{
	Token operatorToken;
	std::unique_ptr<Expression> operand;
	UnaryType unaryType = (UnaryType)0;

	Unary()
	{
		statementType = StatementType::UnaryExpr;
	}

	llvm::Value* Generate() override;
};

enum class BinaryType
{
	Add = 1,  CompoundAdd,
	Subtract, CompoundSub,
	Multiply, CompoundMul,
	Divide,   CompoundDiv,
	Assign,
	Equal, NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

struct Binary : public Expression
{
	BinaryType binaryType = (BinaryType)0;
	std::unique_ptr<Expression> left, right;
	Token operatorToken;

	Binary()
	{
		statementType = StatementType::BinaryExpr;
	}

	llvm::Value* Generate() override;
};

struct Branch : public Statement
{
	std::unique_ptr<Expression> expression;
	std::vector<std::unique_ptr<Statement>> ifBody;
	std::vector<std::unique_ptr<Statement>> elseBody;

	bool HasElse() const { return !elseBody.empty(); }

	Branch()
	{
		statementType = StatementType::Branch;
	}

	llvm::Value* Generate() override;
};

struct Call : public Expression
{
	std::string fnName;
	std::vector<std::unique_ptr<Expression>> args;
	struct FunctionPrototype* target = nullptr;

	Call()
	{
		statementType = StatementType::CallExpr;
	}

	llvm::Value* Generate() override;
};

struct Return : public Expression
{
	std::unique_ptr<Expression> expression;

	Return()
	{
		statementType = StatementType::ReturnExpr;
	}

	llvm::Value* Generate() override;
};

struct FunctionPrototype
{
	struct Parameter
	{
		std::string name;
		Type* type = nullptr;
		bool variadic = false;
	};

	std::string name;
	std::vector<Parameter> params;
	Type* returnType = nullptr;
	int scopeIndex = -1;
};

// Yes, function definitions don't technically express anything, but this just inherits from Expression anyways
struct FunctionDefinition : public Expression
{
	FunctionPrototype prototype;
	std::vector<std::unique_ptr<Statement>> body;

	bool HasBody() const { return body.size(); }

	FunctionDefinition()
	{
		statementType = StatementType::FunctionDefine;
	}

	llvm::Value* Generate() override;
};

struct Import : public Statement
{
	std::unique_ptr<Expression> data;

	Import()
	{
		statementType = StatementType::Import;
	}

	llvm::Value* Generate() override;
};

struct VariableDefinition : public Statement
{
	std::unique_ptr<Expression> initializer;
	Type* type = nullptr;
	std::string name;
	int scope = -1;

	// Additional modifiers
	struct Modifiers
	{
		bool isGlobal = false, isConst = false;
	} modifiers;

	VariableDefinition()
	{
		statementType = StatementType::VariableDefine;
	}

	llvm::Value* Generate() override;
};

struct StructureDefinition : public Statement
{
	std::string name;
	std::vector<std::unique_ptr<VariableDefinition>> members;

	StructureDefinition()
	{
		statementType = StatementType::StructureDefine;
	}

	llvm::Value* Generate() override;
};

struct Load : public Expression
{
	std::string name;
	bool emitInstruction = true; // Whether or not an actual load instruction should be emitted;

	Load()
	{
		statementType = StatementType::LoadExpr;
	}

	llvm::Value* Generate() override;
};

struct Store : public Expression
{
	std::string name;
	std::unique_ptr<Expression> right;
	bool storeIntoLoad = false; // Should we load first, then store into that, or just store directly?

	Store()
	{
		statementType = StatementType::StoreExpr;
	}

	llvm::Value* Generate() override;
};
