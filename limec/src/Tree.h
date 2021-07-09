#pragma once

#include "Lexer.h"
#include "Type.h"

enum class StatementType
{
	Default = 1,
	Compound,
	PrimaryValue, PrimaryString, 
	UnaryExpr, BinaryExpr,
	Branch,
	CallExpr,
	ReturnExpr,
	FunctionDefine,
	VariableDefine,
	StructureDefine,
	MemberReadExpr, VariableReadExpr,
	MemberWriteExpr, VariableWriteExpr,
};

struct Statement
{
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
	Type* type;
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
		int i32 = 0;
		bool b32;
		float f32;
	} value;

	PrimaryValue()
	{
		statementType = StatementType::PrimaryValue;
	}

	llvm::Value* Generate() override;
};

struct PrimaryString : public Primary
{
	std::string value;

	PrimaryString()
	{
		statementType = StatementType::PrimaryString;
	}
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

enum VariableFlags
{
	VariableFlags_None = 0 << 0,
	VariableFlags_Immutable = 1 << 0,
	VariableFlags_Global = 1 << 1,
};

// Yes, function definitions don't technically express anything, but this just inherits from Expression anyways
struct FunctionDefinition : public Expression
{
	struct Parameter
	{
		Type* type;
		std::string name;
		VariableFlags flags = VariableFlags_None;
	};

	std::string name;
	std::vector<Parameter> params;
	std::vector<std::unique_ptr<Statement>> body;
	int scopeIndex = -1; // index into parser's scope container

	FunctionDefinition()
	{
		statementType = StatementType::FunctionDefine;
	}

	llvm::Value* Generate() override;
};

inline VariableFlags operator|(VariableFlags a, VariableFlags b)
{
	return (VariableFlags)((int)a | (int)b);
}
inline VariableFlags operator|=(VariableFlags& a, VariableFlags b)
{
	return (a = (VariableFlags)((int)a | (int)b));
}

struct VariableDefinition : public Statement
{
	std::unique_ptr<Expression> initializer;
	VariableFlags flags = VariableFlags_None;
	Type* type = nullptr;
	std::string name;
	int scope = -1;

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

	Load()
	{
		statementType = StatementType::VariableReadExpr;
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
		statementType = StatementType::VariableWriteExpr;
	}

	llvm::Value* Generate() override;
};