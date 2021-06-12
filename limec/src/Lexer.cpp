#include "Lexer.h"

#include <memory>
#include <string>
#include "Utils.h"

#include "Error.h"

#include <cassert>

static Lexer* lexer;

static bool IsAtEnd()
{
	return *lexer->current == '\0';
}

static bool IsWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static void Advance(int length)
{
	lexer->current += length;
}

static char Peek()
{
	return *lexer->current;
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
	case 'a':
	{
		if (CheckKeyword("and", 3))
		{
			*token = MakeToken(TokenType::And, 3);
			return;
		}
		break;
	}
	case 'b':
	{
		if (CheckKeyword("bool", 4))
		{
			*token = MakeToken(TokenType::Bool, 4);
			return;
		}
		break;
	}
	case 'e':
	{
		if (CheckKeyword("else", 4))
		{
			*token = MakeToken(TokenType::Else, 4);
			return;
		}
		break;
	}
	case 'f':
	{
		if (CheckKeyword("float", 5))
		{
			*token = MakeToken(TokenType::Float, 5);
			return;
		}
		else if (CheckKeyword("false", 5))
		{
			*token = MakeToken(TokenType::False, 5);
			return;
		}
		break;
	}
	case 'i':
	{
		if (CheckKeyword("int", 3))
		{
			*token = MakeToken(TokenType::Int, 3);
			return;
		}
		if (CheckKeyword("if", 2))
		{
			*token = MakeToken(TokenType::If, 2);
			return;
		}
		break;
	}
	case 'm':
	{
		if (CheckKeyword("mut", 3))
		{
			*token = MakeToken(TokenType::Mut, 3);
			return;
		}
		break;
	}
	case 'o':
	{
		if (CheckKeyword("or", 2))
		{
			*token = MakeToken(TokenType::Or, 2);
			return;
		}
		break;
	}
	case 'r':
	{
		if (CheckKeyword("return", 6))
		{
			*token = MakeToken(TokenType::Return, 6);
			return;
		}
		break;
	}
	case 't':
	{
		if (CheckKeyword("true", 4))
		{
			*token = MakeToken(TokenType::True, 4);
			return;
		}
		break;
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
	// Advance
	SkipWhitespace();

	start = current;

	previousToken = currentToken;

	ProcessToken(&currentToken);

	// We don't want the lexer to *actually* advance when we process the next token
	// so instead I use this little hack to revert current to what it was before
	const char* oldCurrent = current;
	int oldLine = line;

	ProcessToken(&nextToken);
	
	current = oldCurrent;
	line = oldLine;

	return currentToken;
}

bool Lexer::Expect(TokenType type)
{
	return currentToken.type == type;
}

Lexer::Lexer(const char* source)
{
	lexer = this;

	current = source;
	start = current;
}