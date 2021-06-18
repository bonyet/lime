#pragma once

#include "Lexer.h"

#include <string>
#include "Utils.h"

#include "Type.h"

struct Statement
{
	virtual ~Statement() {}

	virtual llvm::Value* Generate() { return nullptr; }
};

// Block of statements
struct Compound : public Statement
{
	std::vector<std::unique_ptr<Statement>> statements;
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

	llvm::Value* Generate() override;
};

struct PrimaryString : public Primary
{
	std::string value;
};

enum class UnaryType
{
	Negate = 1,
	PrefixIncrement, PrefixDecrement,
	PostfixIncrement, PostfixDecrement,

	AddressOf, Deref, // TODO: implement
};

struct Unary : public Expression
{
	Token operatorToken;
	std::unique_ptr<Expression> operand;
	UnaryType type = (UnaryType)0;
};

enum class BinaryType
{
	Add = 1,  CompoundAdd,
	Subtract, CompoundSub,
	Multiply, CompoundMul,
	Divide,   CompoundDiv,
	Assign,
	Equal,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

struct Binary : public Expression
{
	BinaryType binaryType = (BinaryType)0;
	Token operatorToken;
	std::unique_ptr<Expression> left, right;

	llvm::Value* Generate() override;
};

struct Branch : public Statement
{
	std::unique_ptr<Expression> expression;
	std::vector<std::unique_ptr<Statement>> ifBody;
	std::vector<std::unique_ptr<Statement>> elseBody;

	bool HasElse() const { return !elseBody.empty(); }

	llvm::Value* Generate() override;
};

struct Call : public Expression
{
	std::string fnName;
	std::vector<std::unique_ptr<Expression>> args;

	llvm::Value* Generate() override;
};

struct Return : public Expression
{
	std::unique_ptr<Expression> expression;

	llvm::Value* Generate() override { return expression->Generate(); }
};

enum VariableFlags : short
{
	VariableFlags_None      = 0 << 0,
	VariableFlags_Immutable = 1 << 0,
	VariableFlags_Mutable   = 1 << 1,
	VariableFlags_Global    = 1 << 2,
	VariableFlags_Reference = 1 << 3,
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
	int indexOfReturnInBody = -1;
	std::vector<std::unique_ptr<Statement>> body;
	int scopeIndex = -1; // index into parser's scope container

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
	VariableFlags flags;
	std::string name;
	int scope = -1;
	Type* type;

	llvm::Value* Generate() override;
};

struct StructureDefinition : public Statement
{
	std::string name;
	std::vector<std::unique_ptr<VariableDefinition>> members;

	llvm::Value* Generate() override;
};

struct VariableRead : public Expression
{
	std::string name;

	llvm::Value* Generate() override;
};

struct VariableWrite : public Expression
{
	std::string name;
	std::unique_ptr<Expression> right;

	llvm::Value* Generate() override;
};