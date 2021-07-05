#include "limecpch.h"

#include "Tree.h"
#include "Typer.h"
#include "Parser.h"

#include <unordered_map>
#include "Scope.h"

static Parser* parser;

using std::unique_ptr;
using std::make_unique;

#define Assert(cond, msg, ...) { if (!(cond)) { throw LimeError(msg, __VA_ARGS__); } }

static const char* reservedIdentifiers[] =
{
	"int", "float", "bool", "string", "void"
};

static std::vector<Scope> scopes = { { } };

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
		throw LimeError("cannot decrease a scope depth of 0");
#endif
	parser->scope = &scopes[--parser->scopeDepth];
}

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
	auto result = parser->scope->namedVariableTypes[name].type;
	Assert(result, "invalid variable type");
	return result;
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

static void SaveState() {
	oldState = parser->state;
}
static void OrState(ParseState state) {
	parser->state |= state;
}
static void ResetState() {
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

	case TokenType::Equal:             return BinaryType::Assign;
	case TokenType::DoubleEqual:       return BinaryType::Equal;
	case TokenType::ExclamationEqual:  return BinaryType::NotEqual;
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
	case BinaryType::NotEqual:
		return 19;
	}

	return 0;
}

static bool IsCompoundAssignmentOp(BinaryType type)
{
	return type == BinaryType::CompoundAdd ||
		type == BinaryType::CompoundSub ||
		type == BinaryType::CompoundMul ||
		type == BinaryType::CompoundDiv;
}

// Forward declarations
static unique_ptr<Statement>  ParseStatement();
static unique_ptr<Expression> ParseFunctionCall();
static unique_ptr<Expression> ParseUnaryExpression();
static unique_ptr<Statement>  ParseCompoundStatement();
static unique_ptr<Expression> ParsePrimaryExpression();
static unique_ptr<Expression> ParseVariableExpression();
static unique_ptr<Expression> ParseMemberAccessExpression();
static unique_ptr<Expression> ParseFunctionDefinition(Token* current);
static unique_ptr<VariableDefinition> ParseVariableDefinitionStatement();

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

	// Groupings
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
		unary->unaryType = type;

		Advance(); // To operand
		unary->operand = ParsePrimaryExpression();

		// Determine the type of the result of the unary expression
		unary->type = unary->operand->type;

		return unary;
	};

	// Prefix unary operators
	switch (token->type)
	{
		case TokenType::Exclamation: return makeUnary(UnaryType::Not);
		case TokenType::Dash:        return makeUnary(UnaryType::Negate);
		case TokenType::Increment:   return makeUnary(UnaryType::PrefixIncrement);
		case TokenType::Decrement:   return makeUnary(UnaryType::PrefixDecrement);

		case TokenType::Ampersand:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::AddressOf;

			auto previousState = parser->state;

			Advance(); // To operand
			unary->operand = ParsePrimaryExpression();
			
			{
				VariableRead* operand = nullptr;
				if ((operand = dynamic_cast<VariableRead*>(unary->operand.get())))
					operand->emitLoad = false;
			}

			// &a
			// The type of the unary expression is not the type of a,
			// it is the type of a pointer to a.
			unary->type = GetType("*" + unary->operand->type->name);
			
			return unary;
		}
		case TokenType::Star:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::Deref;

			Advance(); // To operand
			unary->operand = ParsePrimaryExpression();

			{
				VariableRead* operand = nullptr;
				if ((operand = dynamic_cast<VariableRead*>(unary->operand.get())))
					operand->emitLoad = true;
			}

			// Determine the type of the result of the unary expression
			std::string operandTypeName = unary->operand->type->name;
			unary->type = GetType(operandTypeName.erase(0, 1)); // Remove the first *

			return unary;
		}
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
		unary->unaryType = type;

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
				return ParseFunctionCall();
			else if (next->type == TokenType::Dot)
				return ParseMemberAccessExpression();

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

	throw LimeError("invalid token for primary expression: %.*s", token.length, token.start);
}

static unique_ptr<Statement> ParseExpressionStatement()
{
	PROFILE_FUNCTION();

	unique_ptr<Statement> statement = ParseExpression(-1);
	Expect(TokenType::Semicolon, "expected ';' after expression");

	return statement;
}

static unique_ptr<Expression> ParseFunctionDefinition(Token* nameToken)
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
	Expect(TokenType::LeftParen, "expected '(' after '::'");

	// Parse parameters
	while (current->type != TokenType::RightParen)
	{
		FunctionDefinition::Parameter param;
		param.name = std::string(current->start, current->length);

		Advance(); // Through name
		Expect(TokenType::Colon, "expected ':' after parameter name"); // Through :

		if (current->type == TokenType::Star)
		{
			Advance(); // Through *			
			param.type = GetType("*" + std::string(current->start, current->length));
		}
		else
			param.type = GetType(std::string(current->start, current->length));

		function->params.push_back(param);

		// Register to scope
		RegisterVariable(param.name, param.type, param.flags);

		Advance();
		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "expected ',' after function parameter");
	}

	Expect(TokenType::RightParen, "expected ')'");

	// Handle nondefault return types
	if (current->type == TokenType::RightArrow)
	{
		Advance(); // Through ->

		function->type = GetType(std::string(current->start, current->length));
		
		Advance(); // Through type
	}

	Expect(TokenType::LeftCurlyBracket, "expected '{' after function declaration");

	// Parse body
	int statementIndex = 0;
	bool hasReturnStatement = false;
	while (current->type != TokenType::RightCurlyBracket)
	{
		auto statement = ParseStatement();

		if (dynamic_cast<Return*>(statement.get()))
			hasReturnStatement = true;

		function->body.push_back(std::move(statement));

		statementIndex++;
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "expected '}' after function body");
	
	if (!hasReturnStatement && function->type != GetType("void"))
		throw LimeError("expected a return statement within '%s'", function->name.c_str());

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
			Expect(TokenType::Comma, "expected ',' after argument");
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

	if (parser->lexer->nextToken.type == TokenType::Dot)
	{
		// TODO: defer type resolution until second pass?
		auto member = make_unique<MemberRead>();

		// Accessing a structure member
		Advance(); // To .
		Advance(); // Through .

		member->memberName = std::string(current->start, current->length);

		return member;
	}
	else
	{
		auto variable = make_unique<VariableRead>();
		variable->name = std::string(current->start, current->length);
		variable->type = GetVariableType(variable->name);

		Advance(); // Through identifier

		return variable;
	}
}

static unique_ptr<VariableDefinition> ParseVariableDefinitionStatement()
{
	PROFILE_FUNCTION();

	VariableFlags flags = VariableFlags_None; // Mutable by default

	// We start at the name
	if (parser->current.type == TokenType::Const)
	{
		flags = VariableFlags_Immutable;
		Advance(); // Through 'const'
	}

	Token nameToken = parser->current;

	if (parser->scopeDepth == 0)
		flags |= VariableFlags_Global;

	Advance(); // To : or :=
	
	auto variable = make_unique<VariableDefinition>();
	variable->scope = parser->scopeDepth;
	variable->name = std::string(nameToken.start, nameToken.length);

	if (parser->current.type == TokenType::WalrusTeeth)
	{
		OrState(ParseState::VariableWrite);

		// Automatically deduce variable type
		Advance(); // Through :=

		// Now we are at the expression to assign to the variable
		auto expression = ParseExpression(-1);
		variable->type = expression->type;
		variable->initializer = std::move(expression);

		Expect(TokenType::Semicolon, "expected ';' after expression");
		
		ResetState();
	}
	else
	{
		Token typeToken = Advance();

		if (typeToken.type == TokenType::Star)
		{
			typeToken = Advance(); // Through *
			variable->type = GetType("*" + std::string(typeToken.start, typeToken.length));
		}
		else
		{
			variable->type = GetType(std::string(typeToken.start, typeToken.length));
		}
		
		Advance(); // Through name
	}
	
	variable->flags = flags;

	RegisterVariable(variable->name, variable->type, variable->flags);

	// Handle initializer if there is one
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

		Expect(TokenType::Semicolon, "expected ';' after expression");

		ResetState();
	}

	return variable;
}

static unique_ptr<Expression> ParseMemberAccessExpression()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	std::string variableName = std::string(current->start, current->length);

	Type* type = GetVariableType(variableName);

	Advance(); // Through variable name
	Advance(); // Through .

	std::string memberName = std::string(current->start, current->length);

	auto expr = make_unique<MemberRead>();

	expr->variableTypename = type->name;
	expr->variableName = variableName;
	expr->memberName = memberName;
	expr->type = type;

	Advance(); // Through identifier

	return expr;
}

static unique_ptr<Statement> ParseMemberWriteStatement(const std::string& variableName, Type* type)
{
	Token* current = &parser->current;

	Advance(); // To .
	Advance(); // Through .

	std::string memberName = std::string(current->start, current->length);

	Advance(); // Through id

	// Advance through the = if there is one
	if (current->type == TokenType::Equal)
		Advance();

	auto statement = make_unique<MemberWrite>();

	statement->variableTypename = type->name;
	statement->variableName = variableName;
	statement->memberName = memberName;

	statement->right = ParseExpression(-1);
	statement->type = type;

	Expect(TokenType::Semicolon, "expected ';' after statement");

	return statement;
}

static unique_ptr<Statement> ParseVariableStatement()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	std::string variableName = std::string(current->start, current->length);

	// Creation of variable
	//if (!VariableExistsInScope(variableName))
	//	return ParseVariableDeclarationStatement();

	Type* type = GetVariableType(variableName);

	if (parser->lexer->nextToken.type == TokenType::Dot)
		return ParseMemberWriteStatement(variableName, type);

	Advance(); // Through identifier

	Token operatorToken = *current;
	Advance();

	auto previousState = parser->state;
	OrState(ParseState::VariableWrite);

	auto variable = make_unique<VariableWrite>();
	variable->name = variableName;
	variable->type = type;

	BinaryType binaryOp = GetBinaryType(operatorToken.type);
	if (IsCompoundAssignmentOp(binaryOp))
	{
		OrState(ParseState::VariableWrite);
		
		auto binary = make_unique<Binary>();

		switch (binaryOp)
		{
		case BinaryType::CompoundAdd: binary->binaryType = BinaryType::CompoundAdd; break;
		case BinaryType::CompoundSub: binary->binaryType = BinaryType::CompoundSub; break;
		case BinaryType::CompoundMul: binary->binaryType = BinaryType::CompoundMul; break;
		case BinaryType::CompoundDiv: binary->binaryType = BinaryType::CompoundDiv; break;
		}

		auto left = make_unique<VariableRead>();
		left->name = variableName;
		left->type = type;

		binary->left = std::move(left);
		binary->operatorToken = operatorToken;
		binary->right = ParseExpression(-1);
		binary->type = type;
		
		parser->state = previousState;

		Expect(TokenType::Semicolon, "expected ';' after statement");

		variable->right = std::move(binary);

		return variable;
	}
	else
	{
		variable->right = ParseExpression(-1);

		parser->state = previousState;

		Expect(TokenType::Semicolon, "expected ';' after statement");

		return variable;
	}
}

static unique_ptr<Statement> ParseBranchStatement()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	auto statement = make_unique<Branch>();

	Advance(); // Through 'if'
	
	// Parse the expression
	statement->expression = ParseExpression(-1);

	Expect(TokenType::LeftCurlyBracket, "expected '{' after expression");

	DeepenScope();

	// Parse if body
	while (current->type != TokenType::RightCurlyBracket)
	{
		statement->ifBody.push_back(ParseStatement());
	}

	Expect(TokenType::RightCurlyBracket, "expected '}' after body");

	// Else should be a different scope from if
	IncreaseScope();
	DeepenScope();

	if (current->type == TokenType::Else)
	{
		// We have an else body
		Advance(); // Through else

		Expect(TokenType::LeftCurlyBracket, "expected '{' after else");

		// Parse else body
		while (current->type != TokenType::RightCurlyBracket)
		{
			statement->elseBody.push_back(ParseStatement());
		}

		Expect(TokenType::RightCurlyBracket, "expected '}' after body");
	}

	IncreaseScope();

	return statement;
}

static unique_ptr<Statement> ParseStructureDefinition(Token* nameToken)
{
	PROFILE_FUNCTION();

	// Start at struct keyword

	auto structure = make_unique<StructureDefinition>();
	structure->name = std::string(nameToken->start, nameToken->length);
	auto type = GetType<UserDefinedType>(structure->name);

	Advance(); // Through ::
	Advance(); // Through 'struct'
	Expect(TokenType::LeftCurlyBracket, "expected '{' after 'struct'");

	Token* current = &parser->current;
	
	DeepenScope();
	
	// Parse body
	while (current->type != TokenType::RightCurlyBracket)
	{
		auto decl = ParseVariableDefinitionStatement();
		type->members.push_back({ decl->name, decl->type });
		structure->members.push_back(std::move(decl));
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "expected '}' ater struct body");

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
	case TokenType::Const:
	case TokenType::ID:
	{
		Token* next = &parser->lexer->nextToken;
		switch (next->type)
		{
		case TokenType::WalrusTeeth:
		case TokenType::Colon:
		{
			return ParseVariableDefinitionStatement();
		}
		case TokenType::DoubleColon:
		{
			Token identifier = *token;
			Advance();

			switch (next->type)
			{
			case TokenType::LeftParen:
				return ParseFunctionDefinition(&identifier);
			case TokenType::Struct:
				return ParseStructureDefinition(&identifier);
			default:
				throw CompileError("Invalid declaration");
			}
		}
		case TokenType::LeftParen:
			return ParseFunctionCall();
		default:
			return ParseVariableStatement(); // If all else 'fails', it's a variable statement
		}

		throw LimeError("invalid identifier expression '%.*s'", next->length, next->start);
	}
	case TokenType::If:
		return ParseBranchStatement();
	}

	return ParseExpressionStatement();
}

static unique_ptr<Statement> ParseCompoundStatement()
{
	PROFILE_FUNCTION();

	Expect(TokenType::LeftCurlyBracket, "expect '{' to begin compound statement");

	DeepenScope();

	auto compound = make_unique<Compound>();
	Token* token = &parser->current;

	// Parse the statements in the block
	while (token->type != TokenType::RightCurlyBracket && token->type != TokenType::Eof)
	{
		compound->statements.push_back(ParseStatement());
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "expect '}' to end compound statement");
	return compound;
}

// it just works
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

int LimeError::GetLine() { return parser->lexer->line; }