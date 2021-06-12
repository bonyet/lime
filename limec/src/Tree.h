#pragma once

#include "Lexer.h"
#include "Type.h"

#include <string>
#include "Utils.h"

#include <llvm\IR\Value.h>

struct Statement
{
	virtual ~Statement() {}
	virtual std::string ToString(int& indent) { return "| [Statement]\n"; }

	virtual llvm::Value* Generate() { return nullptr; }
};

// Block of statements
struct Compound : public Statement
{
	std::vector<std::unique_ptr<Statement>> statements;

	std::string ToString(int& indent) override
	{
		std::string string = indent > 0 ? "| [Compound]:\n" : "[Compound]:\n";

		for (auto& statement : statements)
		{
			string += statement->ToString(indent);
		}

		return string;
	}
};

struct Expression : public Statement
{
	Type type = (Type)0;
};

struct Primary : public Expression
{
	Token token;

	std::string ToString(int& indent) override
	{
		return FormatString("| [Primary]: % .*s\n", token.length, token.start);
	}
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

	std::string ToString(int& indent) override
	{
		std::string base = FormatString("%*c| [Primary]: ", indent, ' ');

		switch (type)
		{
		case Type::Int:
			base += FormatString("%i\n", value.i32);
			break;
		case Type::Float:
			base += FormatString("%f\n", value.f32);
			break;
		case Type::Boolean:
			base += FormatString("%s\n", value.b32 ? "true" : "false");
			break;
		}
		
		return base;
	}

	llvm::Value* Generate() override;
};

struct PrimaryString : public Primary
{
	std::string value;

	std::string ToString(int& indent) override
	{
		return FormatString("| [Primary]: %s\n", value.c_str());
	}
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

	const char* GetLabel()
	{
		switch (type)
		{
		case UnaryType::Negate:           return "| [-Unary]:\n%*c";
		case UnaryType::PrefixIncrement:  return "| [++Unary]:\n%*c";
		case UnaryType::PrefixDecrement:  return "| [--Unary]:\n%*c";
		case UnaryType::PostfixIncrement: return "| [Unary++]:\n%*c";
		case UnaryType::PostfixDecrement: return "| [Unary--]:\n%*c";

		case UnaryType::AddressOf: return "| [&Unary]:\n%*c";
		case UnaryType::Deref:     return "| [*Unary]:\n%*c";
		}

		return "<???>";
	}

	std::string ToString(int& indent) override
	{
		std::string result = FormatString(GetLabel(), indent, ' ');
		result += operand->ToString(indent);
		return result;
	}
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

	const char* GetStringType()
	{
		switch (binaryType)
		{
		case BinaryType::Add:          return "Add";
		case BinaryType::Subtract:     return "Subtract";
		case BinaryType::Multiply:     return "Multiply";
		case BinaryType::Divide:       return "Divide";
		case BinaryType::Assign:        return "Assign";
		case BinaryType::Equal:        return "Equal";
		case BinaryType::Less:         return "Less";
		case BinaryType::LessEqual:    return "LessEqual";
		case BinaryType::Greater:      return "Greater";
		case BinaryType::GreaterEqual: return "GreaterEqual";
		}

		return "<???>";
	}

	std::string ToString(int& indent) override
	{
		std::string result = FormatString("%*c| [%s]:\n%*c", indent, ' ', GetStringType(), indent + 2, ' ');

		result += left->ToString(indent);
		result += FormatString("%*c", indent + 2, ' ');
		result += right->ToString(indent);

		return result;
	}

	llvm::Value* Generate() override;
};

struct Branch : public Statement
{
	std::unique_ptr<Expression> expression;
	std::vector<std::unique_ptr<Statement>> ifBody;
	std::vector<std::unique_ptr<Statement>> elseBody;

	bool hasElse = false;

	std::string ToString(int& indent) override
	{
		std::string base = FormatString("%*c| [Branch, %d]", indent, ' ', (int)hasElse);

		return base;
	}

	llvm::Value* Generate() override;
};

struct Call : public Expression
{
	std::string fnName;
	std::vector<std::unique_ptr<Expression>> args;

	std::string ToString(int& indent) override
	{
		std::string base = FormatString("| [call %s]:\n", fnName.c_str());

		for (int i = 0; i < args.size(); ++i)
		{
			base += args[i]->ToString(indent);
		}

		return base;
	}

	llvm::Value* Generate() override;
};

struct Return : public Expression
{
	std::unique_ptr<Expression> expression;

	std::string ToString(int& indent) override
	{
		std::string base = "| [return]:\n";

		indent += 2;
		base += expression->ToString(indent);
		indent -= 2;

		return base;
	}

	llvm::Value* Generate() override { return expression->Generate(); }
};

// Yes, function declarations don't technically express anything, but this just inherits from Expression anyways
struct FunctionDefinition : public Expression
{
	struct Parameter
	{
		std::string name;
		Type type = (Type)0;
	};

	std::string name;
	std::vector<Parameter> params;
	int indexOfReturnInBody = -1;
	std::vector<std::unique_ptr<Statement>> body;
	int scopeIndex = -1; // index into parser's scope container

	std::string ToString(int& indent) override
	{
		std::string base = FormatString("| [%s(", name.c_str());

		for (int i = 0; i < params.size(); ++i)
		{
			base += params[i].name;

			if (params.size() > 1 && i != params.size() - 1)
			{
				base += ", ";
			}
		}

		base += ")]:\n";

		indent += 2;

		int index = 0;
		for (auto& statement : body)
		{
			if (index++ != indexOfReturnInBody)
				base += statement->ToString(indent);
		}

		if (indexOfReturnInBody != -1)
			base += body[indexOfReturnInBody]->ToString(indent);

		indent -= 2;

		return base;
	}

	llvm::Value* Generate() override;
};

enum VariableFlags : int
{
	VariableFlags_None      = 0,
	VariableFlags_Immutable = 1 << 0,
	VariableFlags_Mutable   = 1 << 1,
	VariableFlags_Global    = 1 << 2,
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
	Type type = (Type)0;
	std::string name;
	int scope = -1;

	std::string ToString(int& indent) override
	{
		return FormatString("| [VariableDef]: %s\n", name.c_str());
	}

	llvm::Value* Generate() override;
};

struct Variable : public Expression
{
	std::string name;

	std::string ToString(int& indent) override
	{
		return FormatString("| [VarAccess]: %s\n", name.c_str());
	}

	llvm::Value* Generate() override;
};