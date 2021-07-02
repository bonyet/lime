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

	AddressOf, Deref, // TODO: implement
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

enum VariableFlags : short
{
	VariableFlags_None      = 0 << 0,
	VariableFlags_Immutable = 1 << 0,
	VariableFlags_Mutable   = 1 << 1,
	VariableFlags_Global    = 1 << 2,
	VariableFlags_Pointer   = 1 << 3,
};

// Yes, function declarations don't technically express anything, but this just inherits from Expression anyways
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

struct MemberRead : public Expression
{
	std::string variableTypename, variableName, memberName;
	
	MemberRead()
	{
		statementType = StatementType::MemberReadExpr;
	}

	llvm::Value* Generate() override;
};

struct VariableRead : public Expression
{
	std::string name;

	VariableRead()
	{
		statementType = StatementType::VariableReadExpr;
	}

	llvm::Value* Generate() override;
};

struct MemberWrite : public Expression
{
	std::string variableTypename, variableName, memberName;
	std::unique_ptr<Expression> right;
	
	MemberWrite()
	{
		statementType = StatementType::MemberWriteExpr;
	}

	llvm::Value* Generate() override;
};

struct VariableWrite : public Expression
{
	std::string name;
	std::unique_ptr<Expression> right;

	VariableWrite()
	{
		statementType = StatementType::VariableWriteExpr;
	}

	llvm::Value* Generate() override;
};