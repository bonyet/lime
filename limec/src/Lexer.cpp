#include "Lexer.h"
#include "Error.h"
#include <cassert>

static Lexer* lexer;

static bool IsAtEnd()
{
	return *lexer->current == '\0';
}

static void Advance(int length)
{
	lexer->current += length;
}

static char Peek()
{
	return *lexer->current;
}

static bool IsWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}
static bool IsAlpha(char c)
{
	return (c >= 'a' && c <= 'z') ||
		   (c >= 'A' && c <= 'Z') ||
			c == '_';
}
static bool IsDigit(char c)
{
	return c >= '0' && c <= '9';
}

static void SkipWhitespace()
{
	// Handle comments
	if (lexer->current[0] == '/' && lexer->current[1] == '/')
	{
		while (*lexer->current != '\n' && *lexer->current != '\0')
		{
			Advance(1);
		}
		lexer->line++;
	}

	while (IsWhitespace(*lexer->current))
	{
		if (*lexer->current == '\n')
			lexer->line++;

		Advance(1);
	}
}

static Token MakeIdentifier()
{
	// Advance through identifier name
	while (IsAlpha(Peek()) || IsDigit(Peek()))
		Advance(1);

	Token token;

	token.type = TokenType::ID;
	token.line = lexer->line;
	token.start = lexer->start;
	token.length = (int)(lexer->current - lexer->start);

	return token;
}

static Token MakeNumber()
{
	// Advance through digits
	while (IsDigit(Peek()) || Peek() == '.' || Peek() == 'f')
	{
		Advance(1);
	}

	Token token;

	token.type = TokenType::Number;
	token.line = lexer->line;
	token.start = lexer->start;
	token.length = (int)(lexer->current - lexer->start);

	return token;
}

static Token StringToken()
{
	Advance(1);

	lexer->start = lexer->current; // Manually do this to exclude quotations
	// Advance through chars
	do
	{
		char c = Peek();
		if (c == '\n' || c == '\0')
		{
			throw LimeError("Unterminated string");
		}

		Advance(1);
	} while (Peek() != '\"');

	Token token;

	token.type = TokenType::String;
	token.line = lexer->line;
	token.start = lexer->start;
	token.length = (int)(lexer->current - lexer->start);

	// Advance through closing quote
	Advance(1);

	return token;
}

static Token MakeToken(TokenType type, int length)
{
	Token token;
	
	token.type = type;
	token.line = lexer->line;
	token.start = lexer->start;
	token.length = length;
	
	Advance(length);

	return token;
}

static void IncrementIndex(int* index)
{
	*index += 1;
	if (*index == Lexer::TOKEN_CACHE_SIZE)
		*index = 0;
}

static int GetUndoDistance()
{
	if (lexer->currentIndex >= lexer->bufferIndex)
		return lexer->currentIndex - lexer->bufferIndex;
	else
		return lexer->currentIndex + Lexer::TOKEN_CACHE_SIZE - lexer->bufferIndex;
}

static bool CheckKeyword(const char* keyword, int length)
{
	for (int i = 0; i < length; i++)
	{
		if (lexer->current[i] != keyword[i])
			return false;
	}

	return true;
}

static void ProcessToken(Token* token)
{
	SkipWhitespace();

	switch (*lexer->current)
	{
	case '(': *token = MakeToken(TokenType::LeftParen, 1); return;
	case ')': *token = MakeToken(TokenType::RightParen, 1); return;

	case '{': *token = MakeToken(TokenType::LeftCurlyBracket, 1); return;
	case '}': *token = MakeToken(TokenType::RightCurlyBracket, 1); return;

	case '[': *token = MakeToken(TokenType::LeftSquareBracket, 1); return;
	case ']': *token = MakeToken(TokenType::RightSquareBracket, 1); return;

	case '<':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::LessEqual, 2); 
			return;
		}
		*token = MakeToken(TokenType::LessEqual, 1); 
		return;
	}
	case '>':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::GreaterEqual, 2); 
			return;
		}
		*token = MakeToken(TokenType::Greater, 1); 
		return;
	}
	case '~':  *token = MakeToken(TokenType::Tilde, 1); return;
	case '+':
	{
		switch (lexer->current[1])
		{
		case '+': *token = MakeToken(TokenType::Increment, 2); return;
		case '=': *token = MakeToken(TokenType::PlusEqual, 2); return;
		default: *token = MakeToken(TokenType::Plus, 1); return;
		}
	}
	case '-':
	{
		switch (lexer->current[1])
		{
		case '-': *token = MakeToken(TokenType::Decrement, 2); return;
		case '=': *token = MakeToken(TokenType::DashEqual, 2); return;
		case '>': *token = MakeToken(TokenType::RightArrow, 2); return;
		default: *token = MakeToken(TokenType::Dash, 1); return;
		}
	}
	case '*':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::StarEqual, 2);
			return;
		}
		*token = MakeToken(TokenType::Star, 1); 
		return;
	}
	case '/':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::ForwardSlashEqual, 2); 
			return;
		}
		*token = MakeToken(TokenType::ForwardSlash, 1); 
		return;
	}
	case '\\': *token = MakeToken(TokenType::BackSlash, 1); return;
	case '=':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::DoubleEqual, 2); 
			return;
		}
		*token = MakeToken(TokenType::Equal, 1); 
		return;
	}
	case '!':
	{
		if (lexer->current[1] == '=')
		{
			*token = MakeToken(TokenType::ExclamationEqual, 2); 
			return;
		}
		*token = MakeToken(TokenType::Exclamation, 1); 
		return;
	}
	case ':':
	{
		switch (lexer->current[1])
		{
		case ':': *token = MakeToken(TokenType::DoubleColon, 2); return;
		case '=': *token = MakeToken(TokenType::WalrusTeeth, 2); return;
		default:
			*token = MakeToken(TokenType::Colon, 1); 
			return;
		}
	}
	case ';':  *token = MakeToken(TokenType::Semicolon, 1); return;
	case '.':  *token = MakeToken(TokenType::Dot, 1); return;
	case ',':  *token = MakeToken(TokenType::Comma, 1); return;
	case '?':  *token = MakeToken(TokenType::QuestionMark, 1); return;

	case '&': *token = MakeToken(TokenType::Ampersand, 1); return;
	case '|': *token = MakeToken(TokenType::Pipe, 1); return;
	case '%': *token = MakeToken(TokenType::Percent, 1); return;
	case '@': *token = MakeToken(TokenType::At, 1); return;
	case '#': *token = MakeToken(TokenType::Hashtag, 1); return;

	case '\"': *token = StringToken(); return;

	// Keywords
	case 'i':
	{
		if (CheckKeyword("int", 3))
		{
			*token = MakeToken(TokenType::Int, 3);
			return;
		}
	}
	}

	// Handle end token
	if (IsAtEnd())
	{
		*token = MakeToken(TokenType::Eof, 0);
		return;
	}

	// Handle numbers and identifiers
	if (IsAlpha(Peek()))
	{
		*token = MakeIdentifier();
		return;
	}
	else if (IsDigit(Peek()))
	{
		*token = MakeNumber();
		return;
	}

	// Unknown token type
	Advance(1);
	throw LimeError("Unexpected token '%1s'", lexer->start);
}

Token Lexer::Next()
{
	if (GetUndoDistance() > Lexer::TOKEN_UNDO_COUNT)
	{
		lexer->tokens[lexer->bufferIndex].valid = false;
		IncrementIndex(&lexer->bufferIndex);
	}

	IncrementIndex(&lexer->currentIndex);

	// Advance
	SkipWhitespace();

	start = current;

	Token* token = &lexer->tokens[lexer->currentIndex].token;
	if (!lexer->tokens[lexer->currentIndex].valid) {
		lexer->tokens[lexer->currentIndex].valid = true;

		ProcessToken(token);
	}

	return *token;
}

Token Lexer::Retreat()
{
	lexer->currentIndex--;

	return PeekNext(0);
}

Token Lexer::PeekNext(int amount)
{
	if (amount > TOKEN_PEEK_COUNT)
	{
		exit(6);
	}

	int index = lexer->currentIndex;
	for (int i = 0; i < amount; i++)
	{
		IncrementIndex(&index);

		if (!lexer->tokens[index].valid)
		{
			lexer->tokens[index].valid = true;
			ProcessToken(&lexer->tokens[index].token);
		}
	}
	
	return lexer->tokens[index].token;
}

Token Lexer::PeekPrevious(int depth)
{
	if (GetUndoDistance() > 1)
	{
		int index = lexer->currentIndex;
		if (!lexer->currentIndex)
		{
			index = TOKEN_CACHE_SIZE - 1; // Wrap around if necessary
			index++;
		}

		assert(lexer->tokens[index - 1].valid);
		return lexer->tokens[index - 1].token;
	}

	exit(8);
}

Token Lexer::Consume()
{
	// TODO: Maybe not necessary?
	return {};
}

Token Lexer::Skip(TokenType type)
{
	// TODO: Maybe not necessary?
	return {};
}

bool Lexer::Expect(TokenType type)
{
	return PeekNext(0).type == type;
}

Lexer::Lexer(const char* source)
{
	lexer = this;

	current = source;
	start = current;
}