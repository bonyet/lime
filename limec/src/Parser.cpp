#include <llvm/IR/Value.h>

#include "Error.h"

#include "Tree.h"
#include "Typer.h"
#include "Parser.h"
#include "Profiler.h"

#include <unordered_map>
#include "Scope.h"

static Parser* parser;

using std::unique_ptr;
using std::make_unique;

#define Assert(cond, msg, ...) { if (!(cond)) { throw LimeError(msg, __VA_ARGS__); } }

static const const char* reservedIdentifiers[] =
{
	"int", "float", "bool", "string", "void"
};

static std::vector<Scope> scopes =
{
	{ }
};

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

static void DeepenScope()
{
	parser->scopeDepth++;
	parser->scope = &scopes.emplace_back();
}
static void IncreaseScope()
{
#ifdef _DEBUG
	if (parser->scopeDepth <= 0)
		throw LimeError("Cannot decrease a scope depth of 0");
#endif
	parser->scope = &scopes[--parser->scopeDepth];
}

static int NumScopes() { return (int)scopes.size();  }

static void RegisterVariable(const std::string& name, Type* varType, VariableFlags flags)
{
	parser->scope->namedVariableTypes[name] = { varType, flags };
}
static bool VariableExistsInScope(const std::string& name, Scope* scope = nullptr)
{
	if (scope)
		return scope->namedVariableTypes.count(name) == 1;

	return parser->scope->namedVariableTypes.count(name) == 1;
}

// If the type has not been added yet, it will create it
template<typename As = Type>
static As* GetType(const std::string& typeName)
{
	PROFILE_FUNCTION();

	// Check if it's a reserved type
	const int numReservedIds = sizeof(reservedIdentifiers) / sizeof(const char*);
	for (int i = 0; i < numReservedIds; i++)
	{
		if (typeName == reservedIdentifiers[i])
			return static_cast<As*>(Typer::Get(typeName));
	}

	for (Type* type : Typer::GetAll())
	{
		if (type->name == typeName)
			return static_cast<As*>(type);
	}

	return Typer::Add<As>(typeName);
}

static Type* GetVariableType(const std::string& name)
{
	return parser->scope->namedVariableTypes[name].type;
}

template<typename... Args>
static void Expect(TokenType type, const char* errorMessageFmt, Args&&... args)
{
	if (!parser->lexer->Expect(type))
		throw LimeError(errorMessageFmt, std::forward<Args>(args)...);

	Advance();
}

// TODO: better system for saving parse state
static ParseState oldState = ParseState::Default;

static void SaveState()
{
	oldState = parser->state;
}
static void OrState(ParseState state)
{
	parser->state |= state;
}
static void ResetState()
{
	parser->state = oldState;
}

static bool IsArithmetic(Type type)
{
	return type == Type::int32Type || type == Type::floatType;
}

static BinaryType GetBinaryType(TokenType type)
{
	switch (type)
	{
	case TokenType::PlusEqual: return BinaryType::CompoundAdd;
	case TokenType::Plus:      return BinaryType::Add;
	case TokenType::DashEqual: return BinaryType::CompoundSub;
	case TokenType::Dash:      return BinaryType::Subtract;
	case TokenType::StarEqual: return BinaryType::CompoundMul;
	case TokenType::Star:      return BinaryType::Multiply;
	case TokenType::ForwardSlashEqual: return BinaryType::CompoundDiv;
	case TokenType::ForwardSlash:      return BinaryType::Divide;

	case TokenType::Equal:        return BinaryType::Assign;
	case TokenType::DoubleEqual:  return BinaryType::Equal;
	case TokenType::Less:         return BinaryType::Less;
	case TokenType::LessEqual:    return BinaryType::LessEqual;
	case TokenType::Greater:      return BinaryType::Greater;
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
	case BinaryType::Assign:
	case BinaryType::Equal:
		return 19;
	}

	return 0;
}

// Forward declarations
static unique_ptr<Statement>  ParseStatement();
static unique_ptr<Expression> ParseFunctionCall();
static unique_ptr<Expression> ParseUnaryExpression();
static unique_ptr<Statement>  ParseCompoundStatement();
static unique_ptr<Expression> ParsePrimaryExpression();
static unique_ptr<Expression> ParseVariableExpression();
static unique_ptr<Expression> ParseFunctionDeclaration(Token* current);
static unique_ptr<VariableDefinition> ParseVariableDeclarationStatement();

static unique_ptr<Expression> ParseExpression(int priority) 
{
	PROFILE_FUNCTION();

	bool isExpression = parser->state == ParseState::Expression;

	OrState(ParseState::Expression);

	unique_ptr<Expression> left = ParseUnaryExpression();

	while (1)
	{
		Token token = parser->current;

		BinaryType type = GetBinaryType(token.type);
		int newPriority = GetBinaryPriority(type);

		if (newPriority == 0 || newPriority <= priority) 
		{
			if (!isExpression)
				ResetState();
		
			return left;
		}

		Advance();  // Through operator

		unique_ptr<Binary> binary = make_unique<Binary>();
		binary->binaryType = type;
		binary->type = left->type; // The type of the binary expression is the type of the left operand

		binary->operatorToken = token;
		binary->left = std::move(left);
		binary->right = ParseExpression(newPriority);

		left = std::move(binary);
	}
}

static unique_ptr<Expression> ParseUnaryExpression()
{
	PROFILE_FUNCTION();

	Token* token = &parser->current;

	// Handle groupings
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
		Token next = parser->lexer->nextToken;

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
			return ParsePrimaryExpression(); // Parse a primary expression if we shouldn't parse a unary one
		}

		token = &next;
		
		unique_ptr<Unary> unary = make_unique<Unary>();
		unary->operatorToken = *token;
		unary->type = type;

		unary->operand = ParsePrimaryExpression();
		Advance();

		return unary;
	}
}

static unique_ptr<Expression> ParseReturnStatement()
{
	PROFILE_FUNCTION();

	auto returnExpr = make_unique<Return>();
	returnExpr->expression = ParseExpression(-1);

	return returnExpr;
}

static unique_ptr<Expression> ParsePrimaryExpression()
{
	PROFILE_FUNCTION();

	Token token = parser->current;
	
	switch (token.type)
	{
		case TokenType::ID:
		{
			Token* next = &parser->lexer->nextToken;

			if (next->type == TokenType::LeftParen)
			{
				return ParseFunctionCall();
			}

			return ParseVariableExpression();
		}
		case TokenType::Return:
		{
			Advance();
			return ParseReturnStatement();
		}
		case TokenType::True:
		{
			Advance();

			auto primary = make_unique<PrimaryValue>();
			primary->value.b32 = true;
			primary->type = Type::boolType;
			return primary;
		}
		case TokenType::False:
		{
			Advance();

			auto primary = make_unique<PrimaryValue>();
			primary->value.b32 = false;
			primary->type = Type::boolType;
			return primary;
		}
		case TokenType::Number:
		{
			Advance();

			auto primary = make_unique<PrimaryValue>();
			primary->token = token;

			// Integral or floating point?
			if (strnchr(token.start, '.', token.length))
			{
				primary->value.f32 = strtof(token.start, nullptr);
				primary->type = Type::floatType;
			}
			else
			{
				primary->value.i32 = (int)strtol(token.start, nullptr, 0);
				primary->type = Type::int32Type;
			}
			
			return primary;
		}
		case TokenType::String:
		{
			Advance();

			auto primary = make_unique<PrimaryString>();
			primary->type = Type::stringType;
			primary->token = token;

			primary->value = std::string(token.start, token.length);
			return primary;
		}
	}

	throw LimeError("Invalid token for primary expression: %.*s", token.length, token.start);
}

static unique_ptr<Statement> ParseExpressionStatement()
{
	PROFILE_FUNCTION();

	unique_ptr<Statement> statement = ParseExpression(-1);
	Expect(TokenType::Semicolon, "Expected ';' after expression");

	return statement;
}

static unique_ptr<Expression> ParseFunctionDeclaration(Token* nameToken)
{
	PROFILE_FUNCTION();

	auto function = make_unique<FunctionDefinition>();

	// Find name
	function->name = std::string(nameToken->start, nameToken->length);
	function->type = Type::voidType; // By default, functions return void
	function->scopeIndex = parser->scopeDepth;

	DeepenScope();

	Token* current = &parser->current;

	Advance(); // Through ::
	Expect(TokenType::LeftParen, "Expected '(' after '::'");

	// Parse parameters
	while (current->type != TokenType::RightParen)
	{
		FunctionDefinition::Parameter param;
		param.type = GetType(std::string(current->start, current->length));

		Advance(); // Through type

		if (current->type == TokenType::Ampersand)
		{
			// By reference
			param.flags |= VariableFlags_Reference;
			Advance(); // Through & 
		}

		param.name = std::string(current->start, current->length);
		function->params.push_back(param);

		// Register to scope
		RegisterVariable(param.name, param.type, VariableFlags_None);

		Advance();
		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "Expected ',' after function parameter");
	}

	Expect(TokenType::RightParen, "Expected ')'");

	// Handle nondefault return types
	if (current->type == TokenType::RightArrow)
	{
		Advance(); // Through ->

		function->type = GetType(std::string(current->start, current->length));
		
		Advance(); // Through type
	}

	Expect(TokenType::LeftCurlyBracket, "Expected '{' after function declaration");

	// Parse body
	int statementIndex = 0;
	while (current->type != TokenType::RightCurlyBracket)
	{
		auto statement = ParseStatement();

		if (dynamic_cast<Return*>(statement.get()))
			function->indexOfReturnInBody = statementIndex;

		function->body.push_back(std::move(statement));

		statementIndex++;
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "Expected '}' after function body");
	
	if (function->indexOfReturnInBody == -1 && function->type != GetType("void"))
	{
		throw LimeError("Expected a return statement within '%s'", function->name.c_str());
	}

	return function;
}

static unique_ptr<Expression> ParseFunctionCall()
{
	PROFILE_FUNCTION();

	// Whether or not this function call is being used as an argument in another function call (nested?)
	bool isArgument    = parser->state & ParseState::FuncCallArgs;
	bool isInitializer = parser->state & ParseState::VariableWrite;

	Token* current = &parser->current;

	auto call = make_unique<Call>();
	call->fnName = std::string(current->start, current->length);

	Advance(); // Through name
	Advance(); // Through (

	ParseState previousState = parser->state;
	OrState(ParseState::FuncCallArgs);
	// Parse arguments
	while (current->type != TokenType::RightParen)
	{
		SaveState();
		call->args.push_back(ParseExpression(-1));
		ResetState();

		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "Expected ',' after argument");
	}
	Advance(); // Through )

	parser->state = previousState;

	// If we are not an argument, reset state and advance through semicolon
	// Also make sure we are not initializing a variable so that we don't advance through the semicolon we need
	if (!isArgument && !isInitializer)
		Advance();

	return call;
}

static unique_ptr<Expression> ParseVariableExpression()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	auto variable = make_unique<VariableRead>();
	variable->name = std::string(current->start, current->length);
	variable->type = GetVariableType(variable->name);

	Advance(); // Through identifier

	return variable;
}

static unique_ptr<VariableDefinition> ParseVariableDeclarationStatement()
{
	PROFILE_FUNCTION();

	VariableFlags flags = VariableFlags_Immutable; // Immutable by default

	// We start at the type (`int`, `float`, etc)
	Token startToken = parser->current;
	if (startToken.type == TokenType::Mut)
	{
		flags = VariableFlags_Mutable;
		startToken = Advance(); // To type
	}

	if (parser->scopeDepth == 0)
		flags |= VariableFlags_Global;

	Token nameToken = Advance();

	Advance(); // Through name

	auto variable = make_unique<VariableDefinition>();
	variable->scope = parser->scopeDepth;
	variable->name = std::string(nameToken.start, nameToken.length);
	variable->type = GetType(std::string(startToken.start, startToken.length));
	variable->flags = flags;

	RegisterVariable(variable->name, variable->type, variable->flags);

	// No initializer
	if (parser->current.type == TokenType::Semicolon)
	{
		Advance(); // Through ;
		return variable;
	}
	else if (parser->current.type == TokenType::Equal)
	{
		OrState(ParseState::VariableWrite);

		Advance(); // Through =

		// Parse initializer
		variable->initializer = ParseExpression(-1);

		Expect(TokenType::Semicolon, "Expected ';' after expression");

		ResetState();
	}

	return variable;
}

static unique_ptr<Statement> ParseMemberAccessStatement()
{
	PROFILE_FUNCTION();


}

static unique_ptr<Statement> ParseVariableStatement()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	std::string name = std::string(current->start, current->length);

	// Creation of variable
	if (!VariableExistsInScope(name))
		return ParseVariableDeclarationStatement();

	Type* type = GetVariableType(name);

	if (parser->lexer->nextToken.type == TokenType::Dot)
	{
		Advance(); // To .
		Advance(); // Through .

		std::string memberName = std::string(current->start, current->length);

		Advance(); // Through id

		// Advance through the = if there is one
		if (current->type == TokenType::Equal)
			Advance();

		auto statement = make_unique<MemberWrite>();
		statement->variableName = name;
		statement->memberName = memberName;
		statement->right = ParseExpression(-1);
		statement->type = type;

		Expect(TokenType::Semicolon, "Expected ';' after statement");

		return statement;
	}

	auto variable = make_unique<VariableWrite>();
	variable->name = name;
	variable->type = type;

	Advance(); // Through identifier

	Advance(); // Through =

	auto previousState = parser->state;
	OrState(ParseState::VariableWrite);
	variable->right = ParseExpression(-1);
	parser->state = previousState;

	Expect(TokenType::Semicolon, "Expected ';' after statement");

	return variable;
}

static unique_ptr<Statement> ParseBranchStatement()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	auto statement = make_unique<Branch>();

	Advance(); // Through 'if'
	
	// Parse the expression
	statement->expression = ParseExpression(-1);

	Expect(TokenType::LeftCurlyBracket, "Expected '{' after expression");

	DeepenScope();

	// Parse if body
	while (current->type != TokenType::RightCurlyBracket)
	{
		statement->ifBody.push_back(ParseStatement());
	}

	Expect(TokenType::RightCurlyBracket, "Expected '}' after body");

	// Else should be a different scope from if
	IncreaseScope();
	DeepenScope();

	if (current->type == TokenType::Else)
	{
		// We have an else body
		Advance(); // Through else

		Expect(TokenType::LeftCurlyBracket, "Expected '{' after else");

		// Parse else body
		while (current->type != TokenType::RightCurlyBracket)
		{
			statement->elseBody.push_back(ParseStatement());
		}

		Expect(TokenType::RightCurlyBracket, "Expected '}' after body");
	}

	IncreaseScope();

	return statement;
}

static unique_ptr<Statement> ParseStructureDeclaration(Token* nameToken)
{
	PROFILE_FUNCTION();

	// Start at struct keyword

	auto structure = make_unique<StructureDefinition>();
	structure->name = std::string(nameToken->start, nameToken->length);
	auto type = GetType<UserDefinedType>(structure->name);

	Advance(); // Through ::
	Advance(); // Through 'struct'
	Expect(TokenType::LeftCurlyBracket, "Expected '{' after 'struct'");

	Token* current = &parser->current;
	
	DeepenScope();
	
	// Parse body
	while (current->type != TokenType::RightCurlyBracket)
	{
		auto decl = ParseVariableDeclarationStatement();
		type->memberTypes.push_back(decl->type);
		structure->members.push_back(std::move(decl));
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "Expected '}' ater struct body");

	return structure;
}

static unique_ptr<Statement> ParseStatement()
{
	PROFILE_FUNCTION();

	Token* token = &parser->current;

	switch (token->type)
	{
	case TokenType::LeftCurlyBracket:
		return ParseCompoundStatement();
	case TokenType::Mut:
	case TokenType::ID:
	{
		Token* next = &parser->lexer->nextToken;
		switch (next->type)
		{
		case TokenType::DoubleColon:
		{
			Token identifier = *token;
			Advance();

			switch (next->type)
			{
			case TokenType::LeftParen:
				return ParseFunctionDeclaration(&identifier);
			case TokenType::Struct:
				return ParseStructureDeclaration(&identifier);
			default:
				throw CompileError("Invalid declaration");
			}
		}
		case TokenType::LeftParen:
			return ParseFunctionCall();
		default:
			return ParseVariableStatement();
		}

		throw LimeError("Invalid identifier expression '%.*s'", next->length, next->start);
	}
	case TokenType::If:
		return ParseBranchStatement();
	}

	return ParseExpressionStatement();
}

static unique_ptr<Statement> ParseCompoundStatement()
{
	PROFILE_FUNCTION();

	Expect(TokenType::LeftCurlyBracket, "Expect '{' to begin compound statement");

	DeepenScope();

	auto compound = make_unique<Compound>();
	Token* token = &parser->current;

	// Parse the statements in the block
	while (token->type != TokenType::RightCurlyBracket && token->type != TokenType::Eof)
	{
		compound->statements.push_back(ParseStatement());
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "Expect '}' to end compound statement");
	return compound;
}

// Parses an entire file
static unique_ptr<Compound> ParseModule()
{
	PROFILE_FUNCTION();

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
	PROFILE_FUNCTION();

	ParseResult result{};

	try
	{
		parser = this;
		parser->lexer = lexer;
		parser->scope = &scopes[0];

		Advance(); // Lex the first token
		
		auto compound = ParseModule();
		
		result.Succeeded = true;
		result.module = std::move(compound);
	}
	catch (LimeError& error)
	{
		fprintf(stderr, "\n%s\n", error.message.c_str());

		result.Succeeded = false;
		result.module = nullptr;
	}

	return result;
}