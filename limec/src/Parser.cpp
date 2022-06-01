#include "limecpch.h"

#include "Tree.h"
#include "Parser.h"

#include <unordered_map>
#include "Scope.h"

#include "PlatformUtils.h"

static Parser* parser;

using std::unique_ptr;
using std::make_unique;

static const char* reservedIdentifiers[] =
{
	"int8", "int32", "int64", "float", "bool", "string", "void",
};

static std::vector<Scope> scopes = { { } };
static std::vector<Call*> functionCallExpressions = {}; // Ugly but necessary
static std::unordered_map<std::string, FunctionPrototype*> declaredFunctions = {};

static Token Advance() 
{
	return (parser->current = parser->lexer->Next());
}

// Advances and returns the 'old current' token
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
	if (parser->scopeDepth <= 0)
		throw LimeError("cannot decrease a scope depth of 0");

	parser->scope = &scopes[--parser->scopeDepth];
}

static void RegisterVariable(const std::string& name, Type* varType)
{
	parser->scope->namedVariableTypes[name] = { varType };
}

static bool VariableExistsInScope(const std::string& name, Scope* scope = nullptr, bool ignoreGlobal = false)
{
	if (!ignoreGlobal && scopes[0].namedVariableTypes.count(name) == 1)
		return true;

	return scope ? scope->namedVariableTypes.count(name) == 1
		: parser->scope->namedVariableTypes.count(name) == 1;
}

// If the type has not been added yet, it will create it
template<typename As = Type>
static As* GetType(const std::string& typeName)
{
	PROFILE_FUNCTION();

	// TODO: revisit

	// Check if it's a reserved type
	const int numReservedIds = sizeof(reservedIdentifiers) / sizeof(const char*);
	for (int i = 0; i < numReservedIds; i++)
	{
		if (typeName == reservedIdentifiers[i])
			return static_cast<As*>(Typer::Get(typeName));
	}

	// If the type already exists, return it
	for (Type* type : Typer::GetAll())
	{
		if (type->name == typeName)
			return static_cast<As*>(type);
	}

	// If it doesn't exist, add it
	return Typer::Add<As>(typeName);
}

// Gets the Type associated with the variable
static Type* GetVariableType(const std::string& name)
{
	Type* type = nullptr;
	if (scopes[0].namedVariableTypes.count(name) == 1)
		type = scopes[0].namedVariableTypes[name].type;
	else
		type = parser->scope->namedVariableTypes[name].type;

	if (!type)
		throw LimeError("undefined variable");

	return type;
}

// Throws if the type of the current token is not of 'type'
// If it does not throw, Advance is called
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

// Returns the binary type corresponding to the TokenType
// For example, TokenType::Plus corresponds to BinaryType::Add
static BinaryType GetBinaryType(TokenType type)
{
	switch (type)
	{
	case TokenType::PlusEqual:         return BinaryType::CompoundAdd;
	case TokenType::Plus:              return BinaryType::Add;
	case TokenType::DashEqual:         return BinaryType::CompoundSub;
	case TokenType::Dash:              return BinaryType::Subtract;
	case TokenType::StarEqual:         return BinaryType::CompoundMul;
	case TokenType::Star:              return BinaryType::Multiply;
	case TokenType::ForwardSlashEqual: return BinaryType::CompoundDiv;
	case TokenType::ForwardSlash:      return BinaryType::Divide;

	case TokenType::Equal:             return BinaryType::Assign;
	case TokenType::DoubleEqual:       return BinaryType::Equal;
	case TokenType::ExclamationEqual:  return BinaryType::NotEqual;
	case TokenType::Less:              return BinaryType::Less;
	case TokenType::LessEqual:         return BinaryType::LessEqual;
	case TokenType::Greater:           return BinaryType::Greater;
	case TokenType::GreaterEqual:      return BinaryType::GreaterEqual;
	}

	return (BinaryType)0;
}

// Higher priorities will be lower in the AST (hopefully?)
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
static unique_ptr<Expression> ParseUnaryExpression();
static unique_ptr<Statement>  ParseCompoundStatement();
static unique_ptr<Expression> ParsePrimaryExpression();
static unique_ptr<Load>       ParseVariableExpression();
static unique_ptr<Store>      ParseVariableStatement(bool consumeSemicolon);

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
		binary->line = parser->lexer->line;
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

	// For convenience
	auto makeUnary = [token](UnaryType type) -> unique_ptr<Unary>
	{
		unique_ptr<Unary> unary = make_unique<Unary>();
		unary->line = parser->lexer->line;
		unary->operatorToken = *token;
		unary->unaryType = type;

		Advance(); // To operand
		unary->operand = ParsePrimaryExpression();

		// Determine the type of the result of the unary expression
		unary->type = unary->operand->type;

		return unary;
	};

	switch (token->type)
	{
		case TokenType::Exclamation:
		{
			unique_ptr<Unary> unary = makeUnary(UnaryType::Not);
			if (unary->operand->statementType == StatementType::LoadExpr)
				static_cast<Load*>(unary->operand.get())->emitInstruction = false;

			return unary;
		}
		case TokenType::Dash:
		{
			unique_ptr<Unary> unary = makeUnary(UnaryType::Negate);
			if (unary->operand->statementType == StatementType::LoadExpr)
				static_cast<Load*>(unary->operand.get())->emitInstruction = false;

			return unary;
		}
		case TokenType::Increment:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->line = parser->lexer->line;
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::PrefixIncrement;

			Advance(); // To operand
			unary->operand = ParsePrimaryExpression();
			unary->type = unary->operand->type;

			if (unary->operand->statementType == StatementType::LoadExpr)
				static_cast<Load*>(unary->operand.get())->emitInstruction = false;

			return unary;
		}
		case TokenType::Decrement:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->line = parser->lexer->line;
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::PrefixDecrement;

			Advance(); // To operand
			unary->operand = ParsePrimaryExpression();
			unary->type = unary->operand->type;

			if (unary->operand->statementType == StatementType::LoadExpr)
				static_cast<Load*>(unary->operand.get())->emitInstruction = false;

			return unary;
		}

		case TokenType::Ampersand:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->line = parser->lexer->line;
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::AddressOf;

			Advance(); // To operand
			auto operand = ParsePrimaryExpression();
			if (operand->statementType == StatementType::LoadExpr)
			{
				Load* load = static_cast<Load*>(operand.get());
				load->emitInstruction = false; // Don't load the unary operand since we want to take its address
			}
			unary->operand = std::move(operand);

			// &a
			// The type of the unary expression is not the type of a,
			// it is the type of a pointer to a.
			unary->type = GetType("*" + unary->operand->type->name);
			
			return unary;
		}
		case TokenType::Star:
		{
			unique_ptr<Unary> unary = make_unique<Unary>();
			unary->line = parser->lexer->line;
			unary->operatorToken = *token;
			unary->unaryType = UnaryType::Deref;

			Advance(); // To operand			
			auto operand = ParsePrimaryExpression();
			bool emitLoad = true;

			if (operand->statementType == StatementType::LoadExpr && 
				parser->lexer->currentToken.type == TokenType::Equal)
			{
				emitLoad = false; // Don't load the unary operand since we want to assign to it
			}
			unary->operand = std::move(operand);
			
			if (emitLoad)
			{
				// Determine the type of the result of the unary expression
				std::string operandTypeName = unary->operand->type->name;
				unary->type = GetType(operandTypeName.erase(0, 1)); // Remove the first *
			}
			else
			{
				Load* load = static_cast<Load*>(unary->operand.get());
				load->emitInstruction = false;

				//unary->type = GetType(unary->operand->type->name.erase(0, 1)); // Remove the first *
				unary->type = GetType(unary->operand->type->name);
			}


			return unary;
		}
	}

	// Handle any postfix unary operators (i++)
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
		unary->line = parser->lexer->line;
		unary->operatorToken = *token;
		unary->unaryType = type;

		unary->operand = ParsePrimaryExpression();

		// Make sure the compiler doesn't explode
		if (unary->operand->statementType == StatementType::LoadExpr)
			static_cast<Load*>(unary->operand.get())->emitInstruction = false;

		Advance();

		return unary;
	}
}

static unique_ptr<Expression> ParseReturnStatement()
{
	PROFILE_FUNCTION();

	auto returnExpr = make_unique<Return>();
	returnExpr->line = parser->lexer->line;
	returnExpr->expression = ParseExpression(-1);

	return returnExpr;
}

static unique_ptr<Expression> ParseFunctionCall()
{
	PROFILE_FUNCTION();

	// Whether or not this function call is being used as an argument in another function call (nested calls?)
	bool isArgument    = parser->state & ParseState::FuncCallArgs;
	bool isInitializer = parser->state & ParseState::VariableWrite;

	Token* current = &parser->current;

	auto call = make_unique<Call>();
	functionCallExpressions.push_back(call.get());
	call->line = parser->lexer->line;
	call->fnName = std::string(current->start, current->length);

	Advance(); // Through name
	Advance(); // Through (

	ParseState previousState = parser->state;
	OrState(ParseState::FuncCallArgs);
	// Parse arguments
	int argNumber = 0;
	while (current->type != TokenType::RightParen)
	{
		argNumber++;

		ParseState lastState = parser->state;
		call->args.push_back(ParseExpression(-1));
		parser->state = lastState;

		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "expected ',' after argument %d", argNumber);
	}
	Advance(); // Through )

	parser->state = previousState;

	// If we are not an argument, reset state and advance through semicolon
	// Also make sure we are not initializing a variable so that we don't advance through the semicolon we need
	if (!isArgument && !isInitializer)
		Advance();

	return call;
}

static unique_ptr<Expression> ParsePrimaryExpression()
{
	PROFILE_FUNCTION();

	Token token = parser->current;
	TokenType tType = token.type;
	if (tType == TokenType::Dash || tType == TokenType::Exclamation)
		token = Advance();
	
	switch (token.type)
	{
		case TokenType::Null:
		{
			auto primary = make_unique<PrimaryValue>();
			primary->line = parser->lexer->line;
			primary->type = Type::int64PtrType;
			primary->value.ip64 = nullptr;
			return primary;
		}
		case TokenType::ID:
		{
			Token* next = &parser->lexer->nextToken;

			if (next->type == TokenType::LeftParen)
				return ParseFunctionCall();

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
			primary->line = parser->lexer->line;
			primary->value.b32 = true;
			primary->type = Type::boolType;
			return primary;
		}
		case TokenType::False:
		{
			Advance();

			auto primary = make_unique<PrimaryValue>();
			primary->line = parser->lexer->line;
			primary->value.b32 = false;
			primary->type = Type::boolType;
			return primary;
		}
		case TokenType::Number:
		{
			Advance();

			auto primary = make_unique<PrimaryValue>();
			primary->line = parser->lexer->line;
			primary->token = token;

			// Integral or floating point?
			if (strnchr(token.start, '.', token.length))
			{
				primary->value.f32 = strtof(token.start, nullptr);
				primary->type = Type::floatType;
			}
			else
			{
				primary->value.i64 = (int)strtol(token.start, nullptr, 0);
				primary->type = Type::int32Type;
			}
			
			return primary;
		}
		case TokenType::String:
		{
			Advance();

			auto primary = make_unique<StringValue>();
			primary->line = parser->lexer->line;
			primary->type = Type::stringType;
			primary->token = token;

			primary->value = std::string(token.start, token.length);
			return primary;
		}
	}

	throw LimeError("invalid token for primary expression '%.*s'", token.length, token.start);
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
	function->line = parser->lexer->line;
	function->prototype.name = std::string(nameToken->start, nameToken->length);
	function->prototype.returnType = Type::voidType; // By default, functions return void
	function->prototype.scopeIndex = parser->scopeDepth;

	DeepenScope();

	Token* current = &parser->current;

	Advance(); // Through ::
	Expect(TokenType::LeftParen, "expected '(' after '::'");

	// Parse the parameters
	while (current->type != TokenType::RightParen)
	{
		FunctionPrototype::Parameter param;

		param.name = std::string(current->start, current->length);

		Advance(); // Through name
		Expect(TokenType::Colon, "expected ':' after parameter name"); // Through :

		// Pointer types (I am aware this is a rather crude way to handle this)
		if (current->type == TokenType::Star) {
			Advance(); // Through *			
			param.type = GetType("*" + std::string(current->start, current->length));
		}
		else {
			param.type = GetType(std::string(current->start, current->length));
		}

		function->prototype.params.push_back(param);

		// Register to scope
		RegisterVariable(param.name, param.type);

		Advance();
		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "expected ',' after function parameter");
	}

	Expect(TokenType::RightParen, "expected ')'");

	if (current->type != TokenType::RightArrow && current->type != TokenType::LeftCurlyBracket)
		throw LimeError("expected '{' or '->'");

	// Handle trailing return types (if there isn't one it will default to void)
	if (current->type == TokenType::RightArrow)
	{
		Advance(); // Through ->

		function->prototype.returnType = GetType(std::string(current->start, current->length));
		
		Advance(); // Through type
	}
	declaredFunctions[function->prototype.name] = &function->prototype;

	Expect(TokenType::LeftCurlyBracket, "expected '{' after function definition");

	// Parse body
	bool hasReturnStatement = false;
	while (current->type != TokenType::RightCurlyBracket)
	{
		auto statement = ParseStatement();

		if (statement->statementType == StatementType::ReturnExpr)
			hasReturnStatement = true;

		function->body.push_back(std::move(statement));
	}

	IncreaseScope();

	Expect(TokenType::RightCurlyBracket, "expected '}' after function body");
	
	if (!hasReturnStatement && function->type != GetType("void"))
		throw LimeError("expected a return statement within function '%s'", function->prototype.name.c_str());

	return function;
}

static unique_ptr<Load> ParseVariableExpression()
{
	PROFILE_FUNCTION();

	// TODO: revisit structure implementation

	Token* current = &parser->current;

	auto variable = make_unique<Load>();
	variable->line = parser->lexer->line;
	variable->name = std::string(current->start, current->length);
	variable->type = GetVariableType(variable->name);

	Advance(); // Through identifier

	return variable;
}

static unique_ptr<Statement> ParseVariableDefinitionStatement()
{
	PROFILE_FUNCTION();

	Token nameToken = parser->current;
	
	Advance(); // To : or :=
	
	auto variable = make_unique<VariableDefinition>();
	variable->line = parser->lexer->line;
	variable->scope = parser->scopeDepth;
	variable->modifiers.isGlobal = parser->scopeDepth == 0;
	variable->name = std::string(nameToken.start, nameToken.length);

	if (parser->current.type == TokenType::WalrusTeeth)
	{
		// Automatically deduce variable type
		OrState(ParseState::VariableWrite);

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

		if (typeToken.type == TokenType::Const)
		{
			variable->modifiers.isConst = true;
			typeToken = Advance(); // Through 'const'
		}

		// Handle pointer type
		if (typeToken.type == TokenType::Star) 
		{
			typeToken = Advance(); // Through *
			variable->type = GetType("*" + std::string(typeToken.start, typeToken.length));
		}
		else {
			variable->type = GetType(std::string(typeToken.start, typeToken.length));
		}
		
		Advance(); // Through name
	}	

	RegisterVariable(variable->name, variable->type);

	// Handle initializer if there is one
	if (parser->current.type == TokenType::Equal)
	{
		OrState(ParseState::VariableWrite);

		Advance(); // Through =

		// Parse initializer
		variable->initializer = ParseExpression(-1);
		if (variable->initializer->type != variable->type) // High iq play
			variable->initializer->type = variable->type;

		Expect(TokenType::Semicolon, "expected ';' after expression");

		ResetState();
	}
	else
	{
		if (!variable->type && !variable->initializer) // Only if we didn't use :=
			Expect(TokenType::Semicolon, "expected ';' after variable declaration");
	}

	return variable;
}

static unique_ptr<FunctionDefinition> ParseFunctionPrototypeDefinition()
{
	Token* current = &parser->current; // Start at ID

	auto function = make_unique<FunctionDefinition>();
	function->line = parser->lexer->line;
	function->prototype.name = std::string(current->start, current->length);
	function->prototype.scopeIndex = parser->scopeDepth;
	function->prototype.returnType = Type::voidType;
	declaredFunctions[function->prototype.name] = &function->prototype;

	// Parse parameters
	// import printf :: (*i8, ...);
	Advance(); // Through ID
	Expect(TokenType::DoubleColon, "expected '::' after function prototype name");
	Expect(TokenType::LeftParen, "expected '(' after '::'");

	// Parse the parameters
	while (current->type != TokenType::RightParen)
	{
		FunctionPrototype::Parameter param;

		// Pointer types
		if (current->type == TokenType::Star) 
		{
			Advance(); // Through *			
			param.type = GetType("*" + std::string(current->start, current->length));
		}
		else 
		{
			if (current->type == TokenType::Ellipse)
				param.variadic = true;
			else
				param.type = GetType(std::string(current->start, current->length));
		}

		function->prototype.params.push_back(param);

		Advance();

		if (current->type != TokenType::RightParen)
			Expect(TokenType::Comma, "expected ',' after function prototype parameter");
	}
	Expect(TokenType::RightParen, "expected ')'");

	// Return types (default = void)
	if (current->type == TokenType::RightArrow)
	{
		Advance(); // Through ->

		function->prototype.returnType = GetType(std::string(current->start, current->length));

		Advance(); // Through type
	}

	Expect(TokenType::Semicolon, "expected ';' after function prototype");

	return function;
}

static unique_ptr<Import> ParseImportStatement()
{
	auto import = make_unique<Import>();
	import->line = parser->lexer->line;

	Advance(); // Through import

	import->data = ParseFunctionPrototypeDefinition();

	return import;
}

static unique_ptr<Store> ParseVariableStatement(bool consumeSemicolon)
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	std::string variableName = std::string(current->start, current->length);

	Type* type = GetVariableType(variableName);

	Advance(); // Through identifier

	Token operatorToken = *current;
	Advance();

	auto previousState = parser->state;
	OrState(ParseState::VariableWrite);

	auto variable = make_unique<Store>();
	variable->line = parser->lexer->line;
	variable->name = variableName;
	variable->type = type;

	BinaryType binaryOp = GetBinaryType(operatorToken.type);
	if (IsCompoundAssignmentOp(binaryOp))
	{
		OrState(ParseState::VariableWrite);
		
		auto binary = make_unique<Binary>();
		binary->line = parser->lexer->line;

		switch (binaryOp)
		{
		case BinaryType::CompoundAdd: binary->binaryType = BinaryType::CompoundAdd; break;
		case BinaryType::CompoundSub: binary->binaryType = BinaryType::CompoundSub; break;
		case BinaryType::CompoundMul: binary->binaryType = BinaryType::CompoundMul; break;
		case BinaryType::CompoundDiv: binary->binaryType = BinaryType::CompoundDiv; break;
		}

		auto left = make_unique<Load>();
		left->line = parser->lexer->line;
		left->name = variableName;
		left->type = type;

		binary->left = std::move(left);
		binary->operatorToken = operatorToken;
		binary->right = ParseExpression(-1);
		binary->type = type;
		
		parser->state = previousState;

		if (consumeSemicolon)
			Expect(TokenType::Semicolon, "expected ';' after statement");

		variable->right = std::move(binary);

		return variable;
	}
	else
	{
		variable->right = ParseExpression(-1);

		parser->state = previousState;
		
		if (consumeSemicolon)
			Expect(TokenType::Semicolon, "expected ';' after statement");

		return variable;
	}
}

static unique_ptr<Branch> ParseBranchStatement()
{
	PROFILE_FUNCTION();

	Token* current = &parser->current;

	auto branch = make_unique<Branch>();
	branch->line = parser->lexer->line;

	Advance(); // Through 'if'
	
	// Parse the expression
	branch->expression = ParseExpression(-1);

	DeepenScope();
	if (current->type == TokenType::LeftCurlyBracket)
	{
		Advance();

		// Parse the if body
		while (current->type != TokenType::RightCurlyBracket)
			branch->ifBody.push_back(ParseStatement());

		Expect(TokenType::RightCurlyBracket, "expected '}' after body");
	}
	else
	{
		branch->ifBody.push_back(ParseStatement());
	}

	// The else is a different scope from if
	IncreaseScope();

	// Handle the else body if there is one
	if (current->type == TokenType::Else)
	{
		Advance(); // Through else

		DeepenScope();
		if (current->type == TokenType::LeftCurlyBracket)
		{
			Advance();

			// Parse the else body
			while (current->type != TokenType::RightCurlyBracket)
				branch->elseBody.push_back(ParseStatement());

			Expect(TokenType::RightCurlyBracket, "expected '}' after body");
		}
		else
		{
			// Single statement in body
			branch->elseBody.push_back(ParseStatement());
		}
		IncreaseScope();
	}

	return branch;
}

static unique_ptr<Statement> ParseStatement()
{
	PROFILE_FUNCTION();

	Token* token = &parser->current;

	switch (token->type)
	{
	case TokenType::LeftCurlyBracket:
		return ParseCompoundStatement();
	case TokenType::Import:
		return ParseImportStatement();
	case TokenType::Const: // TODO: const after id
	case TokenType::ID:
	{
		Token* next = &parser->lexer->nextToken;
		switch (next->type)
		{
		case TokenType::Increment:
		case TokenType::Decrement:
			return ParseExpressionStatement();
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
			default:
				throw LimeError("invalid declaration");
			}
		}
		case TokenType::LeftParen:
			return ParseFunctionCall();
		default:
			return ParseVariableStatement(true); // If all else 'fails', it's a variable statement
		}

		throw LimeError("invalid identifier expression", next->length, next->start);
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
	compound->line = parser->lexer->line;
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

// basically just ParseCompoundStatement() but with some extra spice (actually removed spice?)
static void ParseModule(Compound* compound)
{
	PROFILE_FUNCTION();

	Token* token = &parser->current;
	while (token->type != TokenType::Eof)
	{
		compound->statements.push_back(ParseStatement());
	}
}

static bool AttemptSynchronization()
{
	static const TokenType synchronizationPoints[] =
	{
		TokenType::Semicolon, // Always end of the line
		TokenType::RightCurlyBracket, // End of definition for struct, class, function, enum, etc
	};

	auto isAtSyncPoint = [](Token* token)
	{
		for (int i = 0; i < 2; i++)
		{
			if (token->type == synchronizationPoints[i])
				return true;
		}
		return false;
	};

	Token* current = &parser->current;
	while (!isAtSyncPoint(current) && current->type != TokenType::Eof)
	{
		// Panic! at the disco
		Advance();
	}

	if (parser->current.type != TokenType::Eof) {
		Advance();

		if (isAtSyncPoint(current))
			Advance();

		return true;
	}

	return false;
}

static unique_ptr<Compound> currentModule = nullptr;

// Some random shit we do to the tree after parsing happens
static void ExecutePostParsingOperations(unique_ptr<Compound>& compound)
{
	// Update Call expression types (so that the type of a call expression = return type of its function)
	for (int i = 0; i < functionCallExpressions.size(); i++)
	{
		Call* call = functionCallExpressions[i];

		if (declaredFunctions.count(call->fnName) == 0)
			throw LimeError("function call to '%s' invalid, function not declared", call->fnName.c_str());

		call->type   = declaredFunctions[call->fnName]->returnType;
		call->target = declaredFunctions[call->fnName];
	}

	// Not needed anymore
	functionCallExpressions.clear();
	declaredFunctions.clear();
}

static void BeginParsingProcedure(ParseResult* result, bool totalSuccess = true)
{
	static uint32_t timesCalled = 0;

	try
	{
		if (timesCalled++ == 0)
			Advance();

		ParseModule(currentModule.get());
		if (totalSuccess)
		{
			ExecutePostParsingOperations(currentModule);
			result->Succeeded = true;
		}

		result->module = std::move(currentModule);
	}
	catch (LimeError& error)
	{
		bool syncSucceeded = AttemptSynchronization();

		SetConsoleColor(12);
		fprintf(stderr, "syntax error (line %d): %s\n", error.line, error.message.c_str());
		SetConsoleColor(15);

		if (syncSucceeded)
		{
			// Resume parsing
			BeginParsingProcedure(result, false);
		}
		else
		{
			// The world just exploded
			result->Succeeded = false;
			result->module = nullptr;
		}
	}
}

ParseResult Parser::Parse(Lexer* lexer)
{
	PROFILE_FUNCTION();

	ParseResult result{};

	parser = this;
	parser->lexer = lexer;
	parser->scope = &scopes[0];
		
	currentModule = make_unique<Compound>();
	BeginParsingProcedure(&result);

	return result;
}

int LimeError::GetLine() { return parser->lexer->line; }
int LimeError::GetColumn() { return parser->lexer->column; }