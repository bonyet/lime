#pragma once

#include "Lexer.h"
#include "String.h"
#include "Type.h"

#include <llvm\IR\Value.h>
#include <list>

struct Statement
{
	virtual ~Statement() {}
	virtual String ToString(int& indent) { return "| [Statement]\n"; }

	virtual void* Generate() { return nullptr; }
};

// Block of statements
struct Compound : public Statement
{
	std::vector<std::unique_ptr<Statement>> statements;

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
	Type type = (Type)0;
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

struct PrimaryNumber : public Primary
{
	union
	{
		int i32 = 0;
		bool b32;
		float f32;
	} value;

	String ToString(int& indent) override
	{
		switch (type)
		{
		case Type::Int:
			return String::FromFormat("| [Primary]: %i\n", value.i32);
		case Type::Float:
			return String::FromFormat("| [Primary]: %f\n", value.f32);
		case Type::Boolean:
			const char* strValue = value.b32 ? "true" : "false";
			return String::FromFormat("| [Primary]: %s\n", strValue);
		}
		
		return {};
	}

	void* Generate() override;
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
		String result;

		result.Format("%*c| [%s]:\n%*c", indent, ' ', GetStringType(), indent + 2, ' ');

		result += left->ToString(indent);
		result += String::FromFormat("%*c", indent + 2, ' ');
		result += right->ToString(indent);

		return result;
	}

	void* Generate() override;
};

struct Call : public Expression
{
	String fnName;
	std::vector<std::unique_ptr<Expression>> args;

	String ToString(int& indent) override
	{
		String base = String::FromFormat("| [call %s]:\n", fnName.chars());

		for (int i = 0; i < args.size(); ++i)
		{
			base += args[i]->ToString(indent);
		}

		return base;
	}

	void* Generate() override;
};

struct Return : public Expression
{
	std::unique_ptr<Expression> expression;

	String ToString(int& indent) override
	{
		String base = "| [return]: \n";

		indent += 2;
		base += expression->ToString(indent);
		indent -= 2;

		return base;
	}

	void* Generate() override { return expression->Generate(); }
};

// Yes, function declarations don't technically express anything, but this just inherits from Expression anyways
struct FunctionDefinition : public Expression
{
	struct Parameter
	{
		String name;
		Type type = (Type)0;
	};

	String name;
	std::vector<Parameter> params;
	int indexOfReturnInBody = -1;
	std::vector<std::unique_ptr<Statement>> body;

	String ToString(int& indent) override
	{
		String base = String::FromFormat("| [%s(", name.chars());

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

		for (auto& statement : body)
		{
			base += statement->ToString(indent);
		}

		base += body[indexOfReturnInBody]->ToString(indent);

		indent -= 2;

		return base;
	}

	void* Generate() override;
};

struct VariableDefinition : public Statement
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

	void* Generate() override;
};