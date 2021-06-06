#pragma once

#include <cstdint>

enum class TokenType : uint16_t
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
	// Primitive type keywords, TODO: add more!
	Int,
	Return,

	// Misc
	Ampersand, Pipe, Percent, At, Hashtag, ID, 
	RightArrow,
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
	static constexpr int TOKEN_UNDO_COUNT = 5, TOKEN_PEEK_COUNT = 5;
	static constexpr int TOKEN_CACHE_SIZE = TOKEN_UNDO_COUNT + TOKEN_PEEK_COUNT + 1; // + 1 for current

	Lexer(const char* source);
	
	Token Next();
	Token Retreat();

	Token PeekNext(int amount); // Returns the next token without advancing
	Token PeekPrevious(int depth); // Returns the previous token without reatreating

	Token Consume(); // Return current token then advance
	Token Skip(TokenType type); // Asserts that the current is of type, and then advances
	bool Expect(TokenType type); // Returns whether or not current is of type 'type'

	struct
	{
		Token token;
		bool valid = false;
	} tokens[TOKEN_CACHE_SIZE];
	int currentIndex = 0, bufferIndex = 0;

	int line = 1;
	const char* current; // Current character
	const char* start; // First character of token being lexed
};