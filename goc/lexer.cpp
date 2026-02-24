#include "lexer.h"

// Constructor
Lexer::Lexer(const std::string& input, const std::string& file)
    : source(input), position(0), line(1), column(1), error_flag(false), filename(file) {

    // Initialize comprehensive C++98 keywords
    keywords = {
        // Control flow
        {"if", TokenType::KEYWORD},
        {"else", TokenType::KEYWORD},
        {"while", TokenType::KEYWORD},
        {"for", TokenType::KEYWORD},
        {"do", TokenType::KEYWORD},
        {"switch", TokenType::KEYWORD},
        {"case", TokenType::KEYWORD},
        {"default", TokenType::KEYWORD},
        {"break", TokenType::KEYWORD},
        {"continue", TokenType::KEYWORD},
        {"return", TokenType::KEYWORD},
        {"goto", TokenType::KEYWORD},

        // Exception handling
        {"try", TokenType::KEYWORD},
        {"catch", TokenType::KEYWORD},
        {"throw", TokenType::KEYWORD},

        // OOP
        {"this", TokenType::KEYWORD},
        {"virtual", TokenType::KEYWORD},
        {"explicit", TokenType::KEYWORD},
        {"friend", TokenType::KEYWORD},
        {"inline", TokenType::KEYWORD},
        {"operator", TokenType::KEYWORD},
        {"template", TokenType::KEYWORD},
        {"typename", TokenType::KEYWORD},
        {"mutable", TokenType::KEYWORD},

        // Namespace
        {"namespace", TokenType::KEYWORD},
        {"using", TokenType::KEYWORD},

        // Casting
        {"dynamic_cast", TokenType::KEYWORD},
        {"static_cast", TokenType::KEYWORD},
        {"const_cast", TokenType::KEYWORD},
        {"reinterpret_cast", TokenType::KEYWORD},
        {"typeid", TokenType::KEYWORD},

        // Memory management
        {"new", TokenType::KEYWORD},
        {"delete", TokenType::KEYWORD},
        {"sizeof", TokenType::KEYWORD},

        // Other
        {"asm", TokenType::KEYWORD},
        {"export", TokenType::KEYWORD},
        {"wchar_t", TokenType::KEYWORD},
        {"bool", TokenType::KEYWORD},
        {"true", TokenType::KEYWORD},
        {"false", TokenType::KEYWORD}
    };

    // Type keywords
    type_keywords = {
        {"void", TokenType::TYPE_SPECIFIER},
        {"char", TokenType::TYPE_SPECIFIER},
        {"short", TokenType::TYPE_SPECIFIER},
        {"int", TokenType::TYPE_SPECIFIER},
        {"long", TokenType::TYPE_SPECIFIER},
        {"float", TokenType::TYPE_SPECIFIER},
        {"double", TokenType::TYPE_SPECIFIER},
        {"signed", TokenType::TYPE_SPECIFIER},
        {"unsigned", TokenType::TYPE_SPECIFIER},
        {"class", TokenType::TYPE_SPECIFIER},
        {"struct", TokenType::TYPE_SPECIFIER},
        {"union", TokenType::TYPE_SPECIFIER},
        {"enum", TokenType::TYPE_SPECIFIER},
        {"typedef", TokenType::TYPE_SPECIFIER}
    };

    // Access specifiers
    access_specifiers = {
        {"public", TokenType::ACCESS_SPECIFIER},
        {"private", TokenType::ACCESS_SPECIFIER},
        {"protected", TokenType::ACCESS_SPECIFIER}
    };

    // Storage class specifiers
    keywords.insert({
        {"static", TokenType::STORAGE_CLASS},
        {"extern", TokenType::STORAGE_CLASS},
        {"auto", TokenType::STORAGE_CLASS},
        {"register", TokenType::STORAGE_CLASS}
    });

    // Type qualifiers
    keywords.insert({
        {"const", TokenType::TYPE_QUALIFIER},
        {"volatile", TokenType::TYPE_QUALIFIER}
    });

    // Initialize operators (keeping for backward compatibility, but < > will be special-cased)
    single_operators = "+-*/=!&|^%~?";  // REMOVED < > . from here
    multi_char_operators = {
        // Arithmetic
        {"++", TokenType::OPERATOR},
        {"--", TokenType::OPERATOR},
        {"+=", TokenType::OPERATOR},
        {"-=", TokenType::OPERATOR},
        {"*=", TokenType::OPERATOR},
        {"/=", TokenType::OPERATOR},
        {"%=", TokenType::OPERATOR},

        // Comparison
        {"==", TokenType::OPERATOR},
        {"!=", TokenType::OPERATOR},

        // Logical
        {"&&", TokenType::OPERATOR},
        {"||", TokenType::OPERATOR},
        {"!", TokenType::OPERATOR},

        // Bitwise
        {"&=", TokenType::OPERATOR},
        {"|=", TokenType::OPERATOR},
        {"^=", TokenType::OPERATOR},
        {"<<", TokenType::OPERATOR},
        {">>", TokenType::OPERATOR},
        {"<<=", TokenType::OPERATOR},
        {">>=", TokenType::OPERATOR}
    };
}

// Categorize keywords for better parsing
Token Lexer::categorizeKeyword(const std::string& value, int line, int column) {
    // Check access specifiers first
    auto access_it = access_specifiers.find(value);
    if (access_it != access_specifiers.end()) {
        return Token(access_it->second, value, line, column);
    }

    // Check type keywords
    auto type_it = type_keywords.find(value);
    if (type_it != type_keywords.end()) {
        return Token(type_it->second, value, line, column);
    }

    // Check other keywords
    auto keyword_it = keywords.find(value);
    if (keyword_it != keywords.end()) {
        return Token(keyword_it->second, value, line, column);
    }

    // Not a keyword
    return Token(TokenType::IDENTIFIER, value, line, column);
}

// Main analysis method
std::vector<Token> Lexer::tokenize() {
    tokens.clear();
    error_flag = false;
    position = 0;
    line = 1;
    column = 1;

    while (position < source.length()) {
        skipWhitespace();

        if (position >= source.length()) {
            break;
        }

        char current = source[position];
        Token token = getNextToken();

        if (token.type != TokenType::UNKNOWN) {
            tokens.push_back(token);
        }
    }

    tokens.push_back(Token(TokenType::END_OF_FILE, "", line, column));
    return tokens;
}

// Get next token
Token Lexer::getNextToken() {
    char current = source[position];

    // Preprocessor directives
    if (current == '#') {
        return readPreprocessor();
    }

    // Numbers
    if (std::isdigit(current)) {
        return readNumber();
    }

    // Identifiers and keywords
    if (std::isalpha(current) || current == '_') {
        Token ident = readIdentifier();

        // Categorize if it's a keyword
        if (ident.type == TokenType::IDENTIFIER) {
            return categorizeKeyword(ident.value, ident.line, ident.column);
        }
        return ident;
    }

    // Strings
    if (current == '"') {
        return readString();
    }

    // Character literals
    if (current == '\'') {
        return readCharacter();
    }

    // Comments
    if (current == '/') {
        if (position + 1 < source.length()) {
            if (source[position + 1] == '/') {
                return readSingleLineComment();
            } else if (source[position + 1] == '*') {
                return readMultiLineComment();
            }
        }
    }

    // Special handling for < and > (CRITICAL FIX)
    if (current == '<' || current == '>') {
        return readOperator();
    }

    // Other operators
    if (single_operators.find(current) != std::string::npos) {
        return readOperator();
    }

    // Punctuation (includes . : , ; etc.)
    return readPunctuation();
}

// Error check
bool Lexer::hasErrors() const {
    return error_flag;
}

// Print all tokens
void Lexer::printTokens(bool show_tokens) const {
    std::cout << "Lexical analysis of file: " << (filename.empty() ? "input" : filename) << "\n";
    std::cout << "Found " << tokens.size() << " tokens:\n";
    std::cout << "=========================================\n";

    if (show_tokens) {
        for (const auto& token : tokens) {
            std::cout << std::setw(20) << std::left << tokenTypeToString(token.type)
                      << " '" << token.value << "'"
                      << " (line " << token.line << ", column " << token.column << ")\n";
        }
    }
}

// Print statistics
void Lexer::printStatistics() const {
    std::unordered_map<TokenType, int> stats;

    for (const auto& token : tokens) {
        stats[token.type]++;
    }

    std::cout << "\nToken statistics:\n";
    std::cout << "===================\n";

    for (const auto& [type, count] : stats) {
        if (count > 0 && type != TokenType::END_OF_FILE) {
            std::cout << std::setw(20) << std::left << tokenTypeToString(type)
                      << ": " << count << "\n";
        }
    }
}

// Save tokens to file
bool Lexer::saveTokensToFile(const std::string& output_filename) const {
    std::ofstream file(output_filename);
    if (!file.is_open()) {
        return false;
    }

    file << "Tokens from file: " << (filename.empty() ? "input" : filename) << "\n";
    file << "Number of tokens: " << tokens.size() << "\n";
    file << "=========================================\n";

    for (const auto& token : tokens) {
        file << std::setw(20) << std::left << tokenTypeToString(token.type)
             << " '" << token.value << "'"
             << " (line " << token.line << ", column " << token.column << ")\n";
    }

    file.close();
    return true;
}

// Private helper methods

void Lexer::skipWhitespace() {
    while (position < source.length()) {
        char current = source[position];
        if (current == ' ' || current == '\t' || current == '\r') {
            advance();
        } else if (current == '\n') {
            line++;
            column = 1;
            advance();
        } else {
            break;
        }
    }
}

Token Lexer::readNumber() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    bool has_dot = false;
    bool has_exponent = false;

    while (position < source.length()) {
        char current = source[position];

        if (std::isdigit(current)) {
            advance();
        } else if (current == '.' && !has_dot && !has_exponent) {
            has_dot = true;
            advance();
        } else if ((current == 'e' || current == 'E') && !has_exponent) {
            has_exponent = true;
            advance();

            // Handle sign in exponent
            if (position < source.length() &&
                (source[position] == '+' || source[position] == '-')) {
                advance();
            }
        } else {
            break;
        }
    }

    // Check suffixes (f, L, etc.)
    if (position < source.length()) {
        char suffix = std::tolower(source[position]);
        if (suffix == 'f' || suffix == 'l' || suffix == 'u') {
            advance();
            // Handle UL, ULL, LL suffixes
            if (position < source.length()) {
                char next = std::tolower(source[position]);
                if (next == 'l') {
                    advance();
                    if (position < source.length() && std::tolower(source[position]) == 'l') {
                        advance();
                    }
                }
            }
        }
    }

    std::string value = source.substr(start, position - start);
    return Token(TokenType::NUMBER, value, startLine, startColumn);
}

Token Lexer::readIdentifier() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    while (position < source.length() &&
           (std::isalnum(source[position]) || source[position] == '_')) {
        advance();
    }

    std::string value = source.substr(start, position - start);
    return Token(TokenType::IDENTIFIER, value, startLine, startColumn);
}

Token Lexer::readString() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    advance(); // Skip opening quote

    bool escape = false;
    while (position < source.length()) {
        char current = source[position];

        if (escape) {
            escape = false;
            advance();
            continue;
        }

        if (current == '\\') {
            escape = true;
        } else if (current == '"') {
            break;
        } else if (current == '\n') {
            line++;
            column = 1;
        }

        advance();
    }

    if (position < source.length()) {
        advance(); // Skip closing quote
    } else {
        reportError("Unterminated string literal", startLine, startColumn);
    }

    std::string value = source.substr(start + 1, position - start - 2);
    return Token(TokenType::STRING, value, startLine, startColumn);
}

Token Lexer::readCharacter() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    advance(); // Skip opening quote

    bool escape = false;
    while (position < source.length()) {
        char current = source[position];

        if (escape) {
            escape = false;
            advance();
            continue;
        }

        if (current == '\\') {
            escape = true;
        } else if (current == '\'') {
            break;
        }

        advance();
    }

    if (position < source.length()) {
        advance(); // Skip closing quote
    } else {
        reportError("Unterminated character literal", startLine, startColumn);
    }

    std::string value = source.substr(start + 1, position - start - 2);
    return Token(TokenType::CHARACTER, value, startLine, startColumn);
}

Token Lexer::readSingleLineComment() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    // Skip "//"
    advance();
    advance();

    while (position < source.length() && source[position] != '\n') {
        advance();
    }

    std::string value = source.substr(start + 2, position - start - 2);
    return Token(TokenType::COMMENT, value, startLine, startColumn);
}

Token Lexer::readMultiLineComment() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    // Skip "/*"
    advance();
    advance();

    while (position < source.length()) {
        if (source[position] == '\n') {
            line++;
            column = 1;
        } else if (source[position] == '*' &&
                   position + 1 < source.length() &&
                   source[position + 1] == '/') {
            advance(); // *
            advance(); // /
            break;
        }
        advance();
    }

    if (position >= source.length()) {
        reportError("Unterminated multi-line comment", startLine, startColumn);
    }

    std::string value = source.substr(start + 2, position - start - 4);
    return Token(TokenType::COMMENT, value, startLine, startColumn);
}

// COMPLETELY REWRITTEN readOperator() - CRITICAL FIX
Token Lexer::readOperator() {
    size_t startLine = line;
    size_t startColumn = column;
    char current = source[position];

    // === HANDLE < OPERATOR AND VARIANTS ===
    if (current == '<') {
        if (position + 2 < source.length() && source[position + 1] == '<' && source[position + 2] == '=') {
            // <<=
            advance(); advance(); advance();
            return Token(TokenType::OPERATOR, "<<=", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '<') {
            // << (left shift / stream output)
            advance(); advance();
            return Token(TokenType::LEFT_SHIFT, "<<", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '=') {
            // <=
            advance(); advance();
            return Token(TokenType::LESS_EQUAL, "<=", startLine, startColumn);
        }
        // <
        advance();
        return Token(TokenType::LESS, "<", startLine, startColumn);
    }

    // === HANDLE > OPERATOR AND VARIANTS ===
    if (current == '>') {
        if (position + 2 < source.length() && source[position + 1] == '>' && source[position + 2] == '=') {
            // >>=
            advance(); advance(); advance();
            return Token(TokenType::OPERATOR, ">>=", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '>') {
            // >> (right shift / stream input)
            advance(); advance();
            return Token(TokenType::RIGHT_SHIFT, ">>", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '=') {
            // >=
            advance(); advance();
            return Token(TokenType::GREATER_EQUAL, ">=", startLine, startColumn);
        }
        // >
        advance();
        return Token(TokenType::GREATER, ">", startLine, startColumn);
    }

    // === HANDLE -> AND ->* ===
    if (current == '-') {
        if (position + 2 < source.length() && source[position + 1] == '>' && source[position + 2] == '*') {
            // ->*
            advance(); advance(); advance();
            return Token(TokenType::ARROW_STAR, "->*", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '>') {
            // ->
            advance(); advance();
            return Token(TokenType::ARROW, "->", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '-') {
            // --
            advance(); advance();
            return Token(TokenType::OPERATOR, "--", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '=') {
            // -=
            advance(); advance();
            return Token(TokenType::OPERATOR, "-=", startLine, startColumn);
        }
        // -
        advance();
        return Token(TokenType::OPERATOR, "-", startLine, startColumn);
    }

    // === HANDLE :: (SCOPE RESOLUTION) ===
    if (current == ':') {
        if (position + 1 < source.length() && source[position + 1] == ':') {
            advance(); advance();
            return Token(TokenType::SCOPE_RESOLUTION, "::", startLine, startColumn);
        }
        // Single : is handled by readPunctuation
        advance();
        return Token(TokenType::COLON, ":", startLine, startColumn);
    }

    // === HANDLE . AND .* ===
    if (current == '.') {
        if (position + 2 < source.length() && source[position + 1] == '.' && source[position + 2] == '.') {
            // ...
            advance(); advance(); advance();
            return Token(TokenType::ELLIPSIS, "...", startLine, startColumn);
        }
        if (position + 1 < source.length() && source[position + 1] == '*') {
            // .*
            advance(); advance();
            return Token(TokenType::DOT_STAR, ".*", startLine, startColumn);
        }
        // Single . is handled by readPunctuation
        advance();
        return Token(TokenType::DOT, ".", startLine, startColumn);
    }

    // === HANDLE OTHER MULTI-CHAR OPERATORS ===
    if (position + 1 < source.length()) {
        std::string twoChar = source.substr(position, 2);

        // Check multi_char_operators map
        if (multi_char_operators.find(twoChar) != multi_char_operators.end()) {
            advance(); advance();
            return Token(multi_char_operators[twoChar], twoChar, startLine, startColumn);
        }
    }

    // === SINGLE CHARACTER OPERATORS ===
    std::string value = std::string(1, current);
    advance();
    return Token(TokenType::OPERATOR, value, startLine, startColumn);
}

Token Lexer::readPunctuation() {
    size_t startLine = line;
    size_t startColumn = column;
    char current = source[position];

    TokenType type = TokenType::UNKNOWN;
    std::string value = std::string(1, current);

    switch (current) {
        case '(': type = TokenType::LEFT_PAREN; break;
        case ')': type = TokenType::RIGHT_PAREN; break;
        case '{': type = TokenType::LEFT_BRACE; break;
        case '}': type = TokenType::RIGHT_BRACE; break;
        case '[': type = TokenType::LEFT_BRACKET; break;
        case ']': type = TokenType::RIGHT_BRACKET; break;
        case ',': type = TokenType::COMMA; break;
        case ';': type = TokenType::SEMICOLON; break;
        case ':':
            // Check for :: (should be handled in readOperator now)
            if (position + 1 < source.length() && source[position + 1] == ':') {
                advance(); advance();
                return Token(TokenType::SCOPE_RESOLUTION, "::", startLine, startColumn);
            }
            type = TokenType::COLON;
            break;
        case '.':
            // Check for ... and .* (should be handled in readOperator now)
            type = TokenType::DOT;
            break;
        default:
            reportError("Unknown symbol: " + value, startLine, startColumn);
            advance();
            return Token(TokenType::UNKNOWN, value, startLine, startColumn);
    }

    advance();
    return Token(type, value, startLine, startColumn);
}

Token Lexer::readPreprocessor() {
    size_t start = position;
    size_t startLine = line;
    size_t startColumn = column;

    advance(); // Skip '#'

    while (position < source.length() && source[position] != '\n') {
        advance();
    }

    std::string value = source.substr(start, position - start);
    return Token(TokenType::PREPROCESSOR, value, startLine, startColumn);
}

void Lexer::advance() {
    if (position < source.length()) {
        position++;
        column++;
    }
}

void Lexer::reportError(const std::string& message, int errorLine, int errorColumn) {
    std::string file_info = filename.empty() ? "" : " file " + filename;
    std::cerr << "Lexer error" << file_info
              << " (line " << errorLine
              << ", column " << errorColumn << "): " << message << std::endl;
    error_flag = true;
}

std::string Lexer::tokenTypeToString(TokenType type) const {
    switch (type) {
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::KEYWORD: return "KEYWORD";
        case TokenType::STRING: return "STRING";
        case TokenType::CHARACTER: return "CHARACTER";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::DOT: return "DOT";
        case TokenType::LESS: return "LESS";
        case TokenType::GREATER: return "GREATER";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::ARROW: return "ARROW";
        case TokenType::ARROW_STAR: return "ARROW_STAR";
        case TokenType::DOT_STAR: return "DOT_STAR";
        case TokenType::SCOPE_RESOLUTION: return "SCOPE_RESOLUTION";
        case TokenType::ELLIPSIS: return "ELLIPSIS";
        case TokenType::PREPROCESSOR: return "PREPROCESSOR";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::UNKNOWN: return "UNKNOWN";
        case TokenType::ACCESS_SPECIFIER: return "ACCESS_SPECIFIER";
        case TokenType::TYPE_SPECIFIER: return "TYPE_SPECIFIER";
        case TokenType::STORAGE_CLASS: return "STORAGE_CLASS";
        case TokenType::TYPE_QUALIFIER: return "TYPE_QUALIFIER";
        case TokenType::NEWLINE: return "NEWLINE";
        default: return "UNKNOWN";
    }
}

// Read file into string
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
