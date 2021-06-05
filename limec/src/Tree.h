#pragma once

#include "Lexer.h"
#include "String.h"
#include <list>

#include <llvm\IR\Value.h>

struct Statement
{
	virtual ~Statement() {}
	virtual String ToString(int& indent) { return "| [Statement]\n"; }

	virtual llvm::Value* Generate() { return nullptr; }
};

// Block of statements
struct Compound : public Statement
{
	std::list<std::unique_ptr<Statement>> statements;

	String ToString(int& indent) override
	{
		String string = indent > 0 ? "| [Compound]:\n" : "[Compound]:\n";

		for (auto& statement : statements)
		{
			string += statement->ToString(indent);
		}

		return string;
	}
};

struct Expression : public Statement
{
};

struct Primary : public Expression
{
	Token token;

	String ToString(int& indent) override
	{
		String result;
		result.Format("| [Primary]: %.*s\n", token.length, token.start);
		return result;
	}
};

enum class NumberType
{
	Int = 1, Float, Boolean,
};

struct PrimaryNumber : public Primary
{
	union
	{
		int i32 = 0;
		bool b32;
		float f32;
	} value;
	NumberType type = (NumberType)0;

	String ToString(int& indent) override
	{
		switch (type)
		{
		case NumberType::Int:
			return String::FromFormat("| [Primary]: %i\n", value.i32);
		case NumberType::Float:
			return String::FromFormat("| [Primary]: %f\n", value.f32);
		case NumberType::Boolean:
			const char* strValue = value.b32 ? "true" : "false";
			return String::FromFormat("| [Primary]: %s\n", strValue);
		}
		
		return {};
	}

	llvm::Value* Generate() override;
};

struct PrimaryString : public Primary
{
	String string;

	String ToString(int& indent) override
	{
		return String::FromFormat("| [Primary]: %s\n", string.chars());
	}
};

enum class UnaryType
{
	Negate = 1,
	PrefixIncrement, PrefixDecrement,
	PostfixIncrement, PostfixDecrement,
};

struct Unary : public Expression
{
	Token operatorToken;
	std::unique_ptr<Expression> operand;
	UnaryType type = (UnaryType)0;

	const char* GetLabel()
	{
		switch (type)
		{
		case UnaryType::Negate:           return "| [-Unary]:\n%*c";
		case UnaryType::PrefixIncrement:  return "| [++Unary]:\n%*c";
		case UnaryType::PrefixDecrement:  return "| [--Unary]:\n%*c";
		case UnaryType::PostfixIncrement: return "| [Unary++]:\n%*c";
		case UnaryType::PostfixDecrement: return "| [Unary--]:\n%*c";
		}

		return "<???>";
	}

	String ToString(int& indent) override
	{
		String result = String::FromFormat(GetLabel(), indent, ' ');
		result += operand->ToString(indent);
		return result;
	}
};

enum class BinaryType
{
	Add = 1,
	Subtract,
	Multiply,
	Divide,
	Equal,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

struct Binary : public Expression
{
	BinaryType type = (BinaryType)0;
	Token operatorToken;
	std::unique_ptr<Expression> left, right;

	const char* GetStringType()
	{
		switch (type)
		{
		case BinaryType::Add:          return "Add";
		case BinaryType::Subtract:     return "Subtract";
		case BinaryType::Multiply:     return "Multiply";
		case BinaryType::Divide:       return "Divide";
		case BinaryType::Equal:        return "Equal";
		case BinaryType::Less:         return "Less";
		case BinaryType::LessEqual:    return "LessEqual";
		case BinaryType::Greater:      return "Greater";
		case BinaryType::GreaterEqual: return "GreaterEqual";
		}

		return "<???>";
	}

	String ToString(int& indent) override
	{
		indent += 2;

		String result;

		if (indent > 2)
			result.Format("| [%s]:\n%*c", GetStringType(), indent, ' ');
		else
			result.Format("[%s]:\n%*c", GetStringType(), indent, ' ');

		result += left->ToString(indent);
		result += String::FromFormat("%*c", indent, ' ');
		result += right->ToString(indent);

		indent -= 2;

		return result;
	}

	llvm::Value* Generate() override;
};

struct VariableDeclaration : public Statement
{
	String ToString(int& indent) override
	{
		return String::FromFormat("| [VariableDecl]\n");
	}
};

struct Variable : public Expression
{
	String idName;

	String ToString(int& indent) override
	{
		return String::FromFormat("| [IDRead]: %s\n", idName.chars());
	}

	llvm::Value* Generate() override;
};