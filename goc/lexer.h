#ifndef LEXER_H
#define LEXER_H

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <fstream>

enum class TokenType {
    // Basic
    NUMBER,
    IDENTIFIER,
    OPERATOR,
    KEYWORD,
    STRING,
    CHARACTER,
    COMMENT,

    // Brackets
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,

    // Separators
    COMMA,
    SEMICOLON,
    COLON,
    DOT,

    // Comparison/Template (CRITICAL FIX)
    LESS,              // < (for templates and comparison)
    GREATER,           // > (for templates and comparison)
    LESS_EQUAL,        // <=
    GREATER_EQUAL,     // >=
    LEFT_SHIFT,        // <<
    RIGHT_SHIFT,       // >>

    // Arrows
    ARROW,             // ->
    ARROW_STAR,        // ->*
    DOT_STAR,          // .*

    // Special
    END_OF_FILE,
    UNKNOWN,
    PREPROCESSOR,
    NEWLINE,

    // C++ specific
    ACCESS_SPECIFIER,  // public, private, protected
    TYPE_SPECIFIER,    // class, struct, enum, union, int, float, etc.
    STORAGE_CLASS,     // static, extern, auto, register
    TYPE_QUALIFIER,    // const, volatile

    // Additional useful tokens
    SCOPE_RESOLUTION,  // ::
    ELLIPSIS          // ... (variadic)
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
};

class Lexer {
private:
    std::string source;
    size_t position;
    size_t line;
    size_t column;
    std::vector<Token> tokens;
    bool error_flag;
    std::string filename;

    // C++ keywords categorized
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<std::string, TokenType> type_keywords;
    std::unordered_map<std::string, TokenType> access_specifiers;

    // Operators (keeping for backward compatibility, but will use specific tokens)
    std::string single_operators;
    std::unordered_map<std::string, TokenType> multi_char_operators;

    // Private methods
    void skipWhitespace();
    Token readNumber();
    Token readIdentifier();
    Token readString();
    Token readCharacter();
    Token readSingleLineComment();
    Token readMultiLineComment();
    Token readOperator();
    Token readPunctuation();
    Token readPreprocessor();
    void advance();
    void reportError(const std::string& message, int errorLine, int errorColumn);
    std::string tokenTypeToString(TokenType type) const;
    Token categorizeKeyword(const std::string& value, int line, int column);

public:
    Lexer(const std::string& input, const std::string& file = "");

    // Main method
    std::vector<Token> tokenize();

    // Get next token
    Token getNextToken();

    // Error check
    bool hasErrors() const;

    // Print tokens
    void printTokens(bool show_tokens = true) const;

    // Print statistics
    void printStatistics() const;

    // Save tokens to file
    bool saveTokensToFile(const std::string& output_filename) const;
};

// Read file into string
std::string readFile(const std::string& filename);

#endif // LEXER_H