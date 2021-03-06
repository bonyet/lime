#pragma once

enum class TokenType : char
{
	Error, Eof,

	// Brackets
	LeftParen, RightParen, LeftCurlyBracket, RightCurlyBracket, LeftSquareBracket, RightSquareBracket,
	
	// Single character
	Plus, Dash, Star, Equal, Exclamation, ForwardSlash, Tilde,
	BackSlash, Quotation, Dot, Comma, QuestionMark, 
	DoubleEqual, ExclamationEqual, Greater, GreaterEqual, Less, LessEqual,

	// Compound assignment and whatnot
	PlusEqual, DashEqual, StarEqual, ForwardSlashEqual,
	Increment, Decrement,

	// Colon stuff... ew
	Colon, DoubleColon, Semicolon, WalrusTeeth,

	// Keywords
	And, Or, If, Else,
	True, False,
	Return, Const,
	Struct, Class,
	Null, 
	Import,

	// Misc
	Ampersand, Pipe, Percent, At, Hashtag, ID, 
	RightArrow, Ellipse,
	// Literals
	String, Number,
};

struct Token
{
	TokenType type = (TokenType)0;
	const char* start;
	int line = 0, length = 1;
};

struct Lexer
{
	Lexer(const char* source);
	
	Token Next();

	bool Expect(TokenType type); // Returns whether or not current is of type 'type'

	// Used for peeking
	Token previousToken, currentToken, nextToken;

	int line = 1, column = 0;
	const char* current; // Current character
	const char* start; // First character of token being lexed
};