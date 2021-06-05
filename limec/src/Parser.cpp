#include "Parser.h"

#include "TreePrinter.h"
#include "Error.h"

static Parser* parser;

using std::unique_ptr;
using std::make_unique;

int LimeError::GetLine()
{
	return parser->lexer->line;
}

static Token Advance()
{
	return (parser->current = parser->lexer->Next());
}

static Token Consume()
{
	Token token = parser->current;
	Advance();
	return token;
}

template<typename... Args>
static void Expect(TokenType type, const char* errorMessageFmt, Args&&... args)
{
	if (!parser->lexer->Expect(type))
		throw LimeError(errorMessageFmt, std::forward<Args>(args)...);

	Advance();
}

static void SetState(Parser::State state)
{
	parser->state = state;
}
static Parser::State oldState = Parser::State::Default;
static void ResetState()
{
	parser->state = oldState;
}

static BinaryType GetBinaryType(TokenType type)
{
	switch (type)
	{
	case TokenType::PlusEqual:
	case TokenType::Plus: return BinaryType::Add;
	case TokenType::DashEqual:
	case TokenType::Dash: return BinaryType::Subtract;
	case TokenType::StarEqual:
	case TokenType::Star: return BinaryType::Multiply;
	case TokenType::ForwardSlashEqual:
	case TokenType::ForwardSlash: return BinaryType::Divide;

	case TokenType::DoubleEqual: return BinaryType::Equal;
	case TokenType::Less: return BinaryType::Less;
	case TokenType::LessEqual: return BinaryType::LessEqual;
	case TokenType::Greater: return BinaryType::Greater;
	case TokenType::GreaterEqual: return BinaryType::GreaterEqual;
	}

	return (BinaryType)0;
}
static int GetBinaryPriority(BinaryType type)
{
	switch (type)
	{
	case BinaryType::Multiply:
	case BinaryType::Divide:
		return 30;
	case BinaryType::Add:
	case BinaryType::Subtract:
		return 24;
	case BinaryType::Less:
	case BinaryType::LessEqual:
	case BinaryType::Greater:
	case BinaryType::GreaterEqual:
		return 20;
	case BinaryType::Equal:
		return 19;
	}

	return 0;
}

// Forward declarations
static unique_ptr<Statement>  ParseStatement();
static unique_ptr<Expression> ParseUnaryExpression();
static unique_ptr<Statement>  ParseCompoundStatement();
static unique_ptr<Expression> ParsePrimaryExpression();

static unique_ptr<Expression> ParseExpression(int priority) 
{
	unique_ptr<Expression> left = ParseUnaryExpression();

	while (1)
	{
		Token token = parser->current;

		BinaryType type = GetBinaryType(token.type);
		int newPriority = GetBinaryPriority(type);

		if (newPriority == 0 || newPriority <= priority) {
			return left;
		}

		Advance();  // Through operator

		unique_ptr<Binary> binary = make_unique<Binary>();
		binary->type = type;
		binary->operatorToken = token;
		binary->left = std::move(left);
		binary->right = ParseExpression(newPriority);

		left = std::move(binary);
	}
}

static unique_ptr<Expression> ParseUnaryExpression()
{
	Token* token = &parser->current;

	// Grouping
	if (token->type == TokenType::LeftParen)
	{
		Advance(); // Through (
		unique_ptr<Expression> expression = ParseExpression(-1);
		Advance(); // Through )

		return expression;
	}

	// Lambda that creates a unary, advances, parses its operand, then returns it
	auto makeUnary = [token](UnaryType type) -> unique_ptr<Unary>
	{
		unique_ptr<Unary> unary = make_unique<Unary>();
		unary->operatorToken = *token;
		unary->type = type;

		Advance(); // To operand
		unary->operand = ParsePrimaryExpression();
		return unary;
	};

	// Prefix unary operators
	switch (token->type)
	{
		case TokenType::Dash:      return makeUnary(UnaryType::Negate);
		case TokenType::Increment: return makeUnary(UnaryType::PrefixIncrement);
		case TokenType::Decrement: return makeUnary(UnaryType::PrefixDecrement);
	}

	// Suffix unary operators
	{
		Token next = parser->lexer->PeekNext(1);

		UnaryType type = (UnaryType)0;
		switch (next.type)
		{
		case TokenType::Increment:
			type = UnaryType::PostfixIncrement; 
			break;
		case TokenType::Decrement:
			type = UnaryType::PostfixDecrement;
			break;
		default:
			return ParsePrimaryExpression(); // Parse a primary expr if we shouldn't parse a unary one
		}

		// Make token point to the next one
		token = &next;
		
		unique_ptr<Unary> unary = make_unique<Unary>();
		unary->operatorToken = *token;
		unary->type = type;

		unary->operand = ParsePrimaryExpression();
		Advance();

		return unary;
	}
}

static unique_ptr<Expression> ParsePrimaryExpression()
{
	Token token = Consume();
	
	switch (token.type)
	{
		case TokenType::Number:
		{
			auto primary = make_unique<PrimaryNumber>();
			primary->token = token;

			// Integral or floating point?
			if (strnchr(token.start, '.', token.length))
			{
				primary->value.f32 = strtof(token.start, nullptr);
				primary->type = NumberType::Float;
			}
			else
			{
				primary->value.i32 = (int)strtol(token.start, nullptr, 0);
				primary->type = NumberType::Int;
			}
			
			return primary;
		}
		case TokenType::String:
		{
			auto primary = make_unique<PrimaryString>();
			primary->token = token;

			primary->string.Copy(token.start, token.length);
			return primary;
		}
	}

	switch (parser->state)
	{
	case Parser::State::VariableInitializer:
	{
		// Identifiers can be used as primary expressions such as:
		// int b = 1;
		// int a = b; <- b is a variable
		// So we have to handle the special case here
		if (token.type == TokenType::ID)
		{
			auto expression = make_unique<Variable>();
			expression->idName.Copy(token.start, token.length);
			return expression;
		}

		throw LimeError("Expected an expression after variable declaration");
	}
	default:
		throw LimeError("Invalid token for primary expression");
	}
}

static unique_ptr<Statement> ParseExpressionStatement()
{
	unique_ptr<Statement> statement = ParseExpression(-1);
	Expect(TokenType::Semicolon, "Expected ';' after expression");

	return statement;
}

static unique_ptr<Statement> ParseVariableDeclarationStatement()
{
	// We start at the type (`int`, `float`, etc)
	Token typeToken = parser->current;
	Token nameToken = Advance();

	Advance(); // Through name

	auto variableDecl = make_unique<VariableDeclaration>();
	
	// No initializer
	if (parser->current.type == TokenType::Semicolon)
	{
		Advance(); // Through ;
		return variableDecl;
	}
	else if (parser->current.type == TokenType::Equal)
	{
		SetState(Parser::State::VariableInitializer);

		Advance(); // Through =
		
		// Parse initializer
		auto initializerExpression = ParseExpression(-1);

		Expect(TokenType::Semicolon, "Expected ';' after expression");

		ResetState();
	}

	return variableDecl;
}

static unique_ptr<Statement> ParseStatement()
{
	Token* token = &parser->current;

	switch (token->type)
	{
	case TokenType::LeftCurlyBracket:
		return ParseCompoundStatement();
	case TokenType::Int:
		return ParseVariableDeclarationStatement();
	}

	return ParseExpressionStatement();
}

static unique_ptr<Statement> ParseCompoundStatement()
{
	Advance(); // Through {

	auto compound = make_unique<Compound>();
	Token* token = &parser->current;

	// Parse the statements in the block
	while (token->type != TokenType::RightCurlyBracket && token->type != TokenType::Eof)
	{
		compound->statements.push_back(ParseStatement());

		token = &parser->current;
	}

	Advance(); // Through }
	return compound;
}

// Parses an entire file
static unique_ptr<Compound> ParseModule()
{
	auto compound = make_unique<Compound>();

	Token* token = &parser->current;
	while (token->type != TokenType::Eof)
	{
		compound->statements.push_back(ParseStatement());
	}

	return compound;
}

ParseResult Parser::Parse(Lexer* lexer)
{
	parser = this;
	parser->lexer = lexer;

	Advance(); // Lex the first token

	ParseResult result{};

	try
	{
		auto compound = ParseModule();
		PrintStatement(compound.get());
		
		result.Succeeded = true;
		result.module = std::move(compound);
	}
	catch (LimeError& error)
	{
		fprintf(stderr, "\n%s\n", error.message.chars());

		result.Succeeded = false;
		result.module = nullptr;
	}

	return result;
}