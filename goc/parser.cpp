#include "parser.h"
#include <sstream>

// ---------- helpers ----------
std::string indentStr(int n) {
    return std::string(n * 2, ' ');
}

void printQuoted(const std::string &s) {
    std::cout << '"' << s << '"';
}

//------------- dumps --------------
void Literal::dump(int indent) const {
    std::cout << indentStr(indent) << "Literal(";
    printQuoted(value);
    std::cout << ") [" << line << ":" << column << "]\n";
}

void Identifier::dump(int indent) const {
    std::cout << indentStr(indent) << "Identifier(" << name << ") [" << line << ":" << column << "]\n";
}

void UnaryOp::dump(int indent) const {
    std::cout << indentStr(indent) << "UnaryOp(" << op << ") [" << line << ":" << column << "]\n";
    if (operand) operand->dump(indent+1);
}

void BinaryOp::dump(int indent) const {
    std::cout << indentStr(indent) << "BinaryOp(" << op << ") [" << line << ":" << column << "]\n";
    if (left) left->dump(indent+1);
    if (right) right->dump(indent+1);
}

void CallExpr::dump(int indent) const {
    std::cout << indentStr(indent) << "CallExpr [" << line << ":" << column << "]\n";
    if (callee) callee->dump(indent+1);
    for (const auto &a : args) a->dump(indent+1);
}

void MemberAccess::dump(int indent) const {
    std::cout << indentStr(indent) << (arrow ? "MemberAccess->" : "MemberAccess.") << member << " [" << line << ":" << column << "]\n";
    if (object) object->dump(indent+1);
}

void ArraySubscript::dump(int indent) const {
    std::cout << indentStr(indent) << "ArraySubscript [" << line << ":" << column << "]\n";
    if (array) {
        std::cout << indentStr(indent+1) << "Array:\n";
        array->dump(indent+2);
    }
    if (index) {
        std::cout << indentStr(indent+1) << "Index:\n";
        index->dump(indent+2);
    }
}

void ExprStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "ExprStmt [" << line << ":" << column << "]\n";
    if (expr) expr->dump(indent+1);
}

void VarDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "VarDecl(";
    for (size_t i = 0; i < typeTokens.size(); i++) {
        std::cout << typeTokens[i];
        if (i < typeTokens.size() - 1) std::cout << " ";
    }
    std::cout << " " << varName << ") [" << line << ":" << column << "]\n";
    if (init) {
        std::cout << indentStr(indent+1) << "Initializer:\n";
        init->dump(indent+2);
    }
}

void BlockStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "Block [" << line << ":" << column << "]\n";
    for (const auto &s : statements) {
        if (s) s->dump(indent+1);
    }
}

void IfStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "If [" << line << ":" << column << "]\n";
    std::cout << indentStr(indent+1) << "Condition:\n";
    if (cond) cond->dump(indent+2);
    std::cout << indentStr(indent+1) << "Then:\n";
    if (thenBranch) thenBranch->dump(indent+2);
    if (elseBranch) {
        std::cout << indentStr(indent+1) << "Else:\n";
        elseBranch->dump(indent+2);
    }
}

void WhileStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "While [" << line << ":" << column << "]\n";
    if (cond) cond->dump(indent+1);
    if (body) body->dump(indent+1);
}

void ForStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "For [" << line << ":" << column << "]\n";
    if (init) { std::cout << indentStr(indent+1) << "Init:\n"; init->dump(indent+2); }
    if (cond) { std::cout << indentStr(indent+1) << "Cond:\n"; cond->dump(indent+2); }
    if (post) { std::cout << indentStr(indent+1) << "Post:\n"; post->dump(indent+2); }
    if (body) { std::cout << indentStr(indent+1) << "Body:\n"; body->dump(indent+2); }
}

void ReturnStmt::dump(int indent) const {
    std::cout << indentStr(indent) << "Return [" << line << ":" << column << "]\n";
    if (expr) expr->dump(indent+1);
}

// C++ specific dumps
void ClassDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "ClassDecl(" << className << ") [" << line << ":" << column << "]\n";
    if (!baseClasses.empty()) {
        std::cout << indentStr(indent+1) << "BaseClasses: ";
        for (const auto& base : baseClasses) {
            std::cout << base << " ";
        }
        std::cout << "\n";
    }
    for (const auto &m : members) {
        if (m) m->dump(indent+1);
    }
}

void StructDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "StructDecl(" << structName << ") [" << line << ":" << column << "]\n";
    for (const auto &m : members) {
        if (m) m->dump(indent+1);
    }
}

void NamespaceDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "NamespaceDecl(" << name << ") [" << line << ":" << column << "]\n";
    if (body) body->dump(indent+1);
}

void TemplateDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "TemplateDecl [" << line << ":" << column << "]\n";
    std::cout << indentStr(indent+1) << "Params: ";
    for (const auto &p : params) {
        std::cout << p << " ";
    }
    std::cout << "\n";
    if (declaration) declaration->dump(indent+1);
}

void AccessSpec::dump(int indent) const {
    std::cout << indentStr(indent) << "AccessSpec(" << access << ") [" << line << ":" << column << "]\n";
}

void IncludeDirective::dump(int indent) const {
    std::cout << indentStr(indent) << "IncludeDirective("
              << (isSystem ? "<" : "\"") << file << (isSystem ? ">" : "\"")
              << ") [" << line << ":" << column << "]\n";
}

void UsingDirective::dump(int indent) const {
    std::cout << indentStr(indent) << "UsingDirective(" << namespaceName << ") [" << line << ":" << column << "]\n";
}

void FunctionDecl::dump(int indent) const {
    std::cout << indentStr(indent) << "FunctionDecl(";
    for (size_t i = 0; i < returnTypeTokens.size(); i++) {
        std::cout << returnTypeTokens[i];
        if (i < returnTypeTokens.size() - 1) std::cout << " ";
    }
    std::cout << " " << funcName;
    if (isConst) std::cout << " const";
    std::cout << ") [" << line << ":" << column << "]\n";

    std::cout << indentStr(indent+1) << "Params:\n";
    for (const auto &p : params) {
        std::cout << indentStr(indent+2);
        for (size_t i = 0; i < p.first.size(); i++) {
            std::cout << p.first[i];
            if (i < p.first.size() - 1) std::cout << " ";
        }
        std::cout << " " << p.second << "\n";
    }
    if (body) body->dump(indent+1);
}

void Program::dump() const {
    std::cout << "Program AST:\n";
    for (const auto &n : top) {
        if (n) n->dump(1);
    }
}

// --------- Parser implementation ----------
Parser::Parser(const std::vector<Token>& toks) : tokens(toks), idx(0), currentClassName("") {
    // Skip any leading comments
    while (idx < tokens.size() && tokens[idx].type == TokenType::COMMENT) {
        idx++;
    }
}

const Token& Parser::peek() const {
    return tokens[idx];
}

const Token& Parser::previous() const {
    return tokens[idx - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) idx++;
    // Skip comment tokens
    while (!isAtEnd() && peek().type == TokenType::COMMENT) {
        idx++;
    }
    return previous();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType t) const {
    if (isAtEnd()) return false;
    return peek().type == t;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto t : types) {
        if (check(t)) { advance(); return true; }
    }
    return false;
}

void Parser::consume(TokenType t, const std::string &msg) {
    if (check(t)) { advance(); return; }
    error(peek(), msg);
}

void Parser::error(const Token& tok, const std::string& message) const {
    std::ostringstream ss;
    ss << "Parse error at line " << tok.line << " col " << tok.column << ": " << message;
    // Print detailed token context for debugging
    std::cerr << ss.str() << std::endl;
    size_t start = (idx > 5) ? idx - 5 : 0;
    size_t end = idx + 5;
    if (end >= tokens.size()) {
        if (!tokens.empty()) end = tokens.size() - 1;
        else end = 0;
    }
    // DEBUG: // std::cerr << "DEBUG token context around idx=" << idx << ":\n";
    for (size_t i = start; i <= end && i < tokens.size(); ++i) {
        std::cerr << i << ": type=" << static_cast<int>(tokens[i].type)
                  << " '" << tokens[i].value << "' (line " << tokens[i].line
                  << "," << tokens[i].column << ")\n";
    }
    throw std::runtime_error(ss.str());
}

bool Parser::isTypeSpecifier() const {
    return check(TokenType::TYPE_SPECIFIER);
}

bool Parser::isTypeQualifier() const {
    return check(TokenType::TYPE_QUALIFIER);
}

bool Parser::isStorageClass() const {
    return check(TokenType::STORAGE_CLASS);
}

std::vector<std::string> Parser::parseType() {  // TODO: remove debug bullshit
    std::vector<std::string> typeTokens;

    // DEBUG: // std::cerr << "DEBUG parseType: Starting with token '" << peek().value
// DEBUG_CONT:               << "' type: " << static_cast<int>(peek().type) << std::endl;

    // Storage class (static, extern, etc.)
    while (isStorageClass()) {
        typeTokens.push_back(peek().value);
        // DEBUG: // std::cerr << "DEBUG parseType: Consumed storage class '" << peek().value << "'" << std::endl;
        advance();
    }

    // Type qualifiers (const, volatile)
    while (isTypeQualifier()) {
        typeTokens.push_back(peek().value);
        // DEBUG: // std::cerr << "DEBUG parseType: Consumed type qualifier '" << peek().value << "'" << std::endl;
        advance();
    }

    // Base type - type specifier OR user-defined type (identifier)
    if (isTypeSpecifier()) {
        typeTokens.push_back(peek().value);
        // DEBUG: // std::cerr << "DEBUG parseType: Consumed type specifier '" << peek().value << "'" << std::endl;
        advance();
    } else if (check(TokenType::IDENTIFIER) || (check(TokenType::KEYWORD) && (peek().value == "typename" || peek().value == "class"))) {
        // Build qualified name and consume nested template arguments as part of the type
        std::string fullname = peek().value;
        // DEBUG: // std::cerr << "DEBUG parseType: Consumed user-defined type '" << peek().value << "'" << std::endl;
        advance();

        // If we consumed a leading 'typename'/'class', attach the next identifier as the actual type name
        if (fullname == "typename" || fullname == "class") {
            if (check(TokenType::IDENTIFIER)) {
                fullname += " ";
                fullname += peek().value;
                advance();
            }
        }

        // Loop to collect ::qualifiers and <template args>
        while (true) {
            if (check(TokenType::SCOPE_RESOLUTION)) {
                advance(); // consume ::
                if (check(TokenType::IDENTIFIER)) {
                    fullname += "::" + peek().value;
                    advance();
                    continue;
                } else {
                    break;
                }
            }

            if (check(TokenType::LESS)) {
                // collect template argument text until matching '>' (handle nesting)
                int depth = 0;
                std::string templ = "";
                // consume '<'
                templ += "<";
                advance();
                depth = 1;
                while (!isAtEnd() && depth > 0) {
                    if (check(TokenType::LESS)) { templ += "<"; advance(); depth++; continue; }
                    if (check(TokenType::GREATER)) { templ += ">"; advance(); depth--; if (depth == 0) break; else continue; }
                    // Append raw token value (including punctuation)
                    templ += peek().value;
                    advance();
                }
                fullname += templ;
                continue;
            }

            break;
        }

        typeTokens.push_back(fullname);
    }

    // Pointer/reference markers
    while (check(TokenType::OPERATOR) && (peek().value == "*" || peek().value == "&")) {
        typeTokens.push_back(peek().value);
        // DEBUG: // std::cerr << "DEBUG parseType: Consumed pointer/reference '" << peek().value << "'" << std::endl;
        advance();

        // const after pointer: int* const
        while (isTypeQualifier()) {
            typeTokens.push_back(peek().value);
            // DEBUG: // std::cerr << "DEBUG parseType: Consumed qualifier after pointer '" << peek().value << "'" << std::endl;
            advance();
        }
    }

    // DEBUG: // std::cerr << "DEBUG parseType: Ending with token '" << peek().value
// DEBUG_CONT:               << "' type: " << static_cast<int>(peek().type) << std::endl;
    return typeTokens;
}

// Helper for lookahead type parsing without consuming tokens
std::vector<std::string> Parser::parseTypeForLookahead(size_t& pos) const {
    std::vector<std::string> typeTokens;
    size_t tempPos = pos;

    // Storage class
    while (tempPos < tokens.size() &&
           tokens[tempPos].type == TokenType::STORAGE_CLASS) {
        typeTokens.push_back(tokens[tempPos].value);
        tempPos++;
    }

    // Type qualifiers
    while (tempPos < tokens.size() &&
           tokens[tempPos].type == TokenType::TYPE_QUALIFIER) {
        typeTokens.push_back(tokens[tempPos].value);
        tempPos++;
    }

    // Base type: support qualified names and nested template args
    bool hasType = false;
    while (tempPos < tokens.size() &&
           (tokens[tempPos].type == TokenType::TYPE_SPECIFIER ||
            tokens[tempPos].type == TokenType::IDENTIFIER ||
            (tokens[tempPos].type == TokenType::KEYWORD && (tokens[tempPos].value == "typename" || tokens[tempPos].value == "class")))) {
        // Start with base identifier/specifier
        std::string fullname = tokens[tempPos].value;
        tempPos++;

        // Qualified names ::A::B
        while (tempPos < tokens.size() && tokens[tempPos].type == TokenType::SCOPE_RESOLUTION) {
            tempPos++; // consume ::
            if (tempPos < tokens.size() && tokens[tempPos].type == TokenType::IDENTIFIER) {
                fullname += "::" + tokens[tempPos].value;
                tempPos++;
            } else break;
        }

        // Template arguments
        if (tempPos < tokens.size() && tokens[tempPos].type == TokenType::LESS) {
            int depth = 0;
            std::string templ = "<";
            tempPos++; depth = 1;
            while (tempPos < tokens.size() && depth > 0) {
                if (tokens[tempPos].type == TokenType::LESS) { templ += "<"; depth++; tempPos++; continue; }
                if (tokens[tempPos].type == TokenType::GREATER) { templ += ">"; depth--; tempPos++; if (depth == 0) break; else continue; }
                templ += tokens[tempPos].value;
                tempPos++;
            }
            fullname += templ;
        }

        typeTokens.push_back(fullname);
        hasType = true;

        if (tempPos < tokens.size() &&
            tokens[tempPos].type == TokenType::TYPE_SPECIFIER &&
            (tokens[tempPos].value == "long" || tokens[tempPos].value == "short" ||
             tokens[tempPos].value == "signed" || tokens[tempPos].value == "unsigned")) {
            continue;
        }
        break;
    }

    // Pointer/reference
    while (tempPos < tokens.size() &&
           tokens[tempPos].type == TokenType::OPERATOR &&
           (tokens[tempPos].value == "*" || tokens[tempPos].value == "&")) {
        typeTokens.push_back(tokens[tempPos].value);
        tempPos++;
    }

    pos = tempPos; // Update the position
    return typeTokens;
}

// Top-level
Program Parser::parseProgram() {
    Program p;
    while (!isAtEnd()) {
        if (check(TokenType::END_OF_FILE)) break;
        // Skip comments
        if (check(TokenType::COMMENT)) {
            advance();
            continue;
        }
        auto node = parseDeclarationOrStatement();
        if (node) p.top.push_back(std::move(node));
    }
    return p;
}

ASTNodePtr Parser::parseDeclarationOrStatement() {
    // Skip comments
    while (check(TokenType::COMMENT)) {
        advance();
    }
    
    const Token &t = peek();

    // Preprocessor directives
    if (t.type == TokenType::PREPROCESSOR) {
        return parseIncludeDirective();
    }

    // Access specifiers (in class context)
    if (t.type == TokenType::ACCESS_SPECIFIER) {
        return parseAccessSpecifier();
    }

    // STATEMENT keywords - handle these first!
    if (t.type == TokenType::KEYWORD) {
        if (t.value == "return" || t.value == "if" || t.value == "while" ||
            t.value == "for" || t.value == "break" || t.value == "continue" || 
            t.value == "throw" || t.value == "delete" || t.value == "new") {
            return parseStatement();
        }
    }

    // C++ keywords and type tokens (for declarations)
    if (t.type == TokenType::KEYWORD || t.type == TokenType::TYPE_SPECIFIER ||
        t.type == TokenType::STORAGE_CLASS || t.type == TokenType::TYPE_QUALIFIER) {

        // Class declaration
        if (t.value == "class") {
            return parseClass();
        }
        // Struct declaration
        if (t.value == "struct") {
            return parseStruct();
        }
        // Namespace declaration
        if (t.value == "namespace") {
            return parseNamespace();
        }
        // Template declaration
        if (t.value == "template") {
            return parseTemplate();
        }
        // Using directive
        if (t.value == "using") {
            return parseUsingDirective();
        }

        // Use parseTypeForLookahead to detect function declarations after complex types
        size_t lookahead = idx;
        auto tt = parseTypeForLookahead(lookahead);
        // After a type, expect IDENTIFIER then '('
        if (lookahead < tokens.size() && tokens[lookahead].type == TokenType::IDENTIFIER) {
            if (lookahead + 1 < tokens.size() && tokens[lookahead + 1].type == TokenType::LEFT_PAREN) {
                return parseFunctionDeclaration();
            }
        }

        // Otherwise it's a variable declaration
        return parseVarDeclaration();
    }

    // Check for user-defined type declarations (qualified names or templates)
    if (t.type == TokenType::IDENTIFIER) {
        size_t la = idx;
        auto tt = parseTypeForLookahead(la);
        if (!tt.empty()) {
            // If parseTypeForLookahead consumed tokens and the next token is an identifier, treat as declaration
            if (la < tokens.size() && tokens[la].type == TokenType::IDENTIFIER) {
                return parseVarDeclaration();
            }
        }
    }

    return parseStatement();
}

// C++ specific parsing methods
ASTNodePtr Parser::parseClass() {
    Token classTok = peek(); advance(); // consume 'class'

    if (!check(TokenType::IDENTIFIER))
        error(peek(), "Expected class name");

    Token nameTok = peek(); advance();
    auto classDecl = std::make_unique<ClassDecl>(nameTok.value, classTok.line, classTok.column);

    std::string oldClassName = currentClassName;
    currentClassName = nameTok.value;

    // Base classes (optional) - SIMPLIFIED
    if (match({TokenType::COLON})) {
        // Parse base class list: public Base1, private Base2
        while (!check(TokenType::LEFT_BRACE) && !isAtEnd()) {
            // Skip access specifiers
            if (check(TokenType::ACCESS_SPECIFIER) || check(TokenType::KEYWORD)) {
                advance();
            }
            // Get base class name
            if (check(TokenType::IDENTIFIER)) {
                classDecl->baseClasses.push_back(peek().value);
                advance();
            }
            // Skip comma
            if (check(TokenType::COMMA)) advance();
            else break;
        }
    }

    consume(TokenType::LEFT_BRACE, "Expected '{' after class name");

    // Parse class members
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto member = parseClassMember();
        if (member) classDecl->members.push_back(std::move(member));
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after class body");
    consume(TokenType::SEMICOLON, "Expected ';' after class declaration");

    currentClassName = oldClassName;
    return classDecl;
}

ASTNodePtr Parser::parseStruct() {
    Token structTok = peek(); advance(); // consume 'struct'

    if (!check(TokenType::IDENTIFIER))
        error(peek(), "Expected struct name");

    Token nameTok = peek(); advance();
    auto structDecl = std::make_unique<StructDecl>(nameTok.value, structTok.line, structTok.column);

    consume(TokenType::LEFT_BRACE, "Expected '{' after struct name");

    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto member = parseClassMember();
        if (member) structDecl->members.push_back(std::move(member));
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after struct body");
    consume(TokenType::SEMICOLON, "Expected ';' after struct declaration");

    return structDecl;
}

ASTNodePtr Parser::parseNamespace() {
    Token nsTok = peek(); advance(); // consume 'namespace'

    std::string name;
    if (check(TokenType::IDENTIFIER)) {
        name = peek().value;
        advance();
        // support qualified namespace names: A::B::C
        while (check(TokenType::SCOPE_RESOLUTION)) {
            advance(); // consume ::
            if (check(TokenType::IDENTIFIER)) {
                name += "::" + peek().value;
                advance();
            } else {
                break;
            }
        }
    } else {
        name = ""; // anonymous namespace
    }

    // DEBUG: // std::cerr << "DEBUG parseNamespace: name='" << name << "' peek='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
    consume(TokenType::LEFT_BRACE, "Expected '{' after namespace");

    auto body = std::make_unique<BlockStmt>(nsTok.line, nsTok.column);
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto decl = parseDeclarationOrStatement();
        if (decl) body->statements.push_back(std::move(decl));
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}' after namespace body");

    return std::make_unique<NamespaceDecl>(name, std::move(body), nsTok.line, nsTok.column);
}

// FIXED: Template parsing with new LESS/GREATER tokens
ASTNodePtr Parser::parseTemplate() {
    Token templateTok = peek(); advance(); // consume 'template'
    consume(TokenType::LESS, "Expected '<' after template");

    auto params = parseTemplateParams();

    consume(TokenType::GREATER, "Expected '>' after template parameters");

    // Parse the templated declaration
    // First attempt: lookahead using parseTypeForLookahead
    size_t la = idx;
    auto tt = parseTypeForLookahead(la);
    if (la < tokens.size() && tokens[la].type == TokenType::IDENTIFIER && la + 1 < tokens.size() && tokens[la+1].type == TokenType::LEFT_PAREN) {
        ASTNodePtr declaration = parseFunctionDeclaration();
        return std::make_unique<TemplateDecl>(std::move(params), std::move(declaration), templateTok.line, templateTok.column);
    }

    // Fallback: scan forward for IDENTIFIER + LEFT_PAREN before '{' or ';'
    size_t k = idx;
    bool foundFunc = false;
    while (k + 1 < tokens.size()) {
        if (tokens[k].type == TokenType::IDENTIFIER && tokens[k+1].type == TokenType::LEFT_PAREN) { foundFunc = true; break; }
        if (tokens[k].type == TokenType::LEFT_BRACE || tokens[k].type == TokenType::SEMICOLON) break;
        k++;
    }
    if (foundFunc) {
        ASTNodePtr declaration = parseFunctionDeclaration();
        return std::make_unique<TemplateDecl>(std::move(params), std::move(declaration), templateTok.line, templateTok.column);
    }

    ASTNodePtr declaration = parseDeclarationOrStatement();
    return std::make_unique<TemplateDecl>(std::move(params), std::move(declaration),
                                         templateTok.line, templateTok.column);
}

ASTNodePtr Parser::parseFunctionDeclaration() {
    int startLine = peek().line;
    int startCol = peek().column;

    std::vector<std::string> returnType;
    std::string funcName;
    
    // Check for constructor/destructor
    if (check(TokenType::IDENTIFIER) && peek().value == currentClassName) {
        funcName = peek().value;
        advance();
    } else if (check(TokenType::OPERATOR) && peek().value == "~") {
        advance();
        if (!check(TokenType::IDENTIFIER) || peek().value != currentClassName) {
            error(peek(), "Expected class name after '~'");
        }
        funcName = "~" + peek().value;
        advance();
    } else {
        // Regular function
        returnType = parseType();
        // If parseType consumed the function name (e.g., 'auto peek'), handle that
        if (check(TokenType::LEFT_PAREN) && !returnType.empty()) {
            // assume last token in returnType is the function name
            funcName = returnType.back();
            returnType.pop_back();
        } else {
            if (!check(TokenType::IDENTIFIER))
                error(peek(), "Expected function name");
            funcName = peek().value;
            advance();
        }
    }

    auto params = parseFunctionParams();

    // Check for const member function
    bool isConst = false;
    if (check(TokenType::TYPE_QUALIFIER) && peek().value == "const") {
        isConst = true;
        advance();
    }

    ASTNodePtr body = nullptr;
    if (check(TokenType::LEFT_BRACE)) {
        body = parseBlock();
    } else {
        consume(TokenType::SEMICOLON, "Expected ';' or function body");
    }

    auto funcDecl = std::make_unique<FunctionDecl>(returnType, funcName,
                                                   std::move(params), std::move(body),
                                                   startLine, startCol);
    funcDecl->isConst = isConst;
    return funcDecl;
}

ASTNodePtr Parser::parseAccessSpecifier() {
    Token accessTok = peek(); advance();
    consume(TokenType::COLON, "Expected ':' after access specifier");
    return std::make_unique<AccessSpec>(accessTok.value, accessTok.line, accessTok.column);
}

// FIXED: Include directive parsing with new LESS/GREATER tokens
ASTNodePtr Parser::parseIncludeDirective() {
    Token includeTok = peek(); advance(); // consume preprocessor token

    // Parse the preprocessor directive value
    std::string directive = includeTok.value;

    // Extract filename from #include directive
    std::string file;
    bool isSystem = false;

    // Simple parsing of the directive string
    size_t includePos = directive.find("include");
    if (includePos != std::string::npos) {
        std::string rest = directive.substr(includePos + 7);

        // Trim whitespace
        size_t start = rest.find_first_not_of(" \t");
        if (start != std::string::npos) {
            rest = rest.substr(start);

            if (rest[0] == '<') {
                isSystem = true;
                size_t end = rest.find('>');
                if (end != std::string::npos) {
                    file = rest.substr(1, end - 1);
                }
            } else if (rest[0] == '"') {
                isSystem = false;
                size_t end = rest.find('"', 1);
                if (end != std::string::npos) {
                    file = rest.substr(1, end - 1);
                }
            }
        }
    }

    return std::make_unique<IncludeDirective>(file, isSystem, includeTok.line, includeTok.column);
}

ASTNodePtr Parser::parseUsingDirective() {
    Token usingTok = peek(); advance(); // consume 'using'

    if (check(TokenType::KEYWORD) && peek().value == "namespace") {
        advance(); // consume 'namespace'
        if (!check(TokenType::IDENTIFIER))
            error(peek(), "Expected namespace name");

        Token nsName = peek(); advance();
        consume(TokenType::SEMICOLON, "Expected ';' after using directive");

        return std::make_unique<UsingDirective>(nsName.value, usingTok.line, usingTok.column);
    }

    // Handle using declarations (using std::cout;)
    while (!check(TokenType::SEMICOLON) && !isAtEnd()) advance();
    consume(TokenType::SEMICOLON, "Expected ';' after using declaration");
    return nullptr;
}

ASTNodePtr Parser::parseClassMember() {
    // Access specifiers
    if (check(TokenType::ACCESS_SPECIFIER)) {
        return parseAccessSpecifier();
    }

    // Check for constructor
    if (check(TokenType::IDENTIFIER) && peek().value == currentClassName) {
        if (idx + 1 < tokens.size() && tokens[idx + 1].type == TokenType::LEFT_PAREN) {
            return parseFunctionDeclaration();
        }
    }

    // Check for destructor
    if (check(TokenType::OPERATOR) && peek().value == "~") {
        if (idx + 1 < tokens.size() && tokens[idx + 1].type == TokenType::IDENTIFIER &&
            tokens[idx + 1].value == currentClassName) {
            return parseFunctionDeclaration();
        }
    }

    // Member functions and variables
    return parseDeclarationOrStatement();
}

// FIXED: Function parameter parsing with better type handling
std::vector<std::pair<std::vector<std::string>, std::string>> Parser::parseFunctionParams() {
    std::vector<std::pair<std::vector<std::string>, std::string>> params;
    consume(TokenType::LEFT_PAREN, "Expected '(' after function name");

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        // Parse parameter type
        // DEBUG: // std::cerr << "DEBUG parseFunctionParams: about to parse type at token '" << peek().value << "' (" << static_cast<int>(peek().type) << ")" << std::endl;
        std::vector<std::string> paramType = parseType();

        if (paramType.empty()) {
            error(peek(), "Expected type in parameter list");
        }

        // Parse parameter name (optional for function declarations)
        std::string paramName;
        if (check(TokenType::IDENTIFIER)) {
            paramName = peek().value;
            advance();
        }

        params.push_back({paramType, paramName});

        if (!match({TokenType::COMMA})) break;
    }

    consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    return params;
}

// FIXED: Template parameter parsing with new LESS/GREATER tokens
std::vector<std::string> Parser::parseTemplateParams() {
    std::vector<std::string> params;

    while (!check(TokenType::GREATER) && !isAtEnd()) {
        if (check(TokenType::KEYWORD) && (peek().value == "typename" || peek().value == "class")) {
            advance(); // consume 'typename' or 'class'
            if (check(TokenType::IDENTIFIER)) {
                params.push_back(peek().value);
                advance();
                // support default parameter: = T
                if (check(TokenType::OPERATOR) && peek().value == "=") {
                    advance();
                    // consume a simple identifier or type specifier as default
                    if (check(TokenType::IDENTIFIER) || check(TokenType::TYPE_SPECIFIER)) {
                        advance();
                    }
                }
            }
        }

        if (check(TokenType::COMMA)) {
            advance();
        } else {
            break;
        }
    }

    return params;
}

ASTNodePtr Parser::parseVarDeclaration() {
    int startLine = peek().line;
    int startCol = peek().column;

    // DEBUG: Print current state
    // DEBUG: // std::cerr << "DEBUG parseVarDeclaration: Starting at token '" << peek().value
// DEBUG_CONT:               << "' type: " << static_cast<int>(peek().type) << std::endl;

    // Parse type (can be complex: const int*, unsigned long long, etc.)
    std::vector<std::string> type = parseType();

    // DEBUG: Print after parseType
    // DEBUG: // std::cerr << "DEBUG parseVarDeclaration: After parseType, current token '" << peek().value
// DEBUG_CONT:               << "' type: " << static_cast<int>(peek().type) << std::endl;
    // DEBUG: // std::cerr << "DEBUG: Type tokens: ";
    for (const auto& t : type) std::cerr << t << " ";
    std::cerr << std::endl;

    // Must have at least one declarator
    if (!check(TokenType::IDENTIFIER)) {
        // DEBUG: // std::cerr << "DEBUG: ERROR - Expected identifier, got '" << peek().value
// DEBUG_CONT:                   << "' type: " << static_cast<int>(peek().type) << std::endl;
        error(peek(), "Expected identifier after type");
    }

    // Collect one or more declarators separated by commas
    std::vector<ASTNodePtr> decls;

    while (true) {
        Token nameTok = peek(); advance();
        // DEBUG: // std::cerr << "DEBUG: Variable name: " << nameTok.value << std::endl;

        ASTNodePtr init = nullptr;
        bool isArrayDecl = false;
        // Array declarator e.g. arr[5]
        if (check(TokenType::LEFT_BRACKET)) {
            isArrayDecl = true;
            Token br = peek(); advance();
            // For simplicity, capture the size expression
            ASTNodePtr sizeExpr = parseExpression();
            consume(TokenType::RIGHT_BRACKET, "Expected ']' in array declarator");
            (void)sizeExpr;
            // If an initializer follows (e.g. = { ... }) handle it
            if (check(TokenType::OPERATOR) && peek().value == "=") {
                // DEBUG: // std::cerr << "DEBUG: Found = initializer after array declarator" << std::endl;
                advance();
                if (check(TokenType::LEFT_BRACE)) {
                    Token ob = peek(); advance();
                    std::string contents;
                    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
                        contents += peek().value;
                        advance();
                    }
                    consume(TokenType::RIGHT_BRACE, "Expected '}' after initializer list");
                    init = std::make_unique<Literal>(contents, ob.line, ob.column, TokenType::LEFT_BRACE);
                } else {
                    init = parseExpression();
                }
            }
        }
        // Brace initializer list e.g. = {1,2,3}
        else if (check(TokenType::OPERATOR) && peek().value == "=") {
            // DEBUG: // std::cerr << "DEBUG: Found = initializer" << std::endl;
            advance();
            if (check(TokenType::LEFT_BRACE)) {
                Token ob = peek(); advance();
                // Consume until right brace for now
                std::string contents;
                while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
                    contents += peek().value;
                    advance();
                }
                consume(TokenType::RIGHT_BRACE, "Expected '}' after initializer list");
                init = std::make_unique<Literal>(contents, ob.line, ob.column, TokenType::LEFT_BRACE);
            } else {
                init = parseExpression();
            }
        } else if (check(TokenType::LEFT_PAREN)) {
            // DEBUG: // std::cerr << "DEBUG: Found constructor initializer" << std::endl;
            advance();
            std::vector<ASTNodePtr> args;
            while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                args.push_back(parseExpression());
                if (!match({TokenType::COMMA})) break;
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after constructor arguments");
            auto typeName = std::make_unique<Identifier>(type.empty() ? "" : type[0], startLine, startCol);
            init = std::make_unique<CallExpr>(std::move(typeName), std::move(args), startLine, startCol);
        }

        auto varDecl = std::make_unique<VarDecl>(type, nameTok.value, std::move(init), startLine, startCol);
        for (const auto& token : type) {
            if (token == "*") varDecl->isPointer = true;
            if (token == "&") varDecl->isReference = true;
        }
        varDecl->isArray = isArrayDecl;
        decls.push_back(std::move(varDecl));

        if (!match({TokenType::COMMA})) break;
    }

    // Allow declaration without semicolon when used in range-for initializer (followed by ':' or ')')
    if (check(TokenType::SEMICOLON)) {
        consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    }
    // DEBUG: // std::cerr << "DEBUG: Successfully parsed variable declaration(s), count=" << decls.size() << std::endl;

    if (decls.size() == 1) {
        return std::move(decls[0]);
    }

    auto block = std::make_unique<BlockStmt>(startLine, startCol);
    for (auto &d : decls) block->statements.push_back(std::move(d));
    return block;
}

ASTNodePtr Parser::parseStatement() {
    const Token &t = peek();

    // Skip preprocessor (already handled in parseDeclarationOrStatement)
    if (t.type == TokenType::PREPROCESSOR) {
        advance();
        return nullptr;
    }

    // Skip 'using namespace' (already handled)
    if (check(TokenType::KEYWORD) && t.value == "using") {
        return parseDeclarationOrStatement();
    }

    // Control structures
    if (match({TokenType::LEFT_BRACE})) {
        idx--;
        return parseBlock();
    }
    if (check(TokenType::KEYWORD) && peek().value == "if") return parseIf();
    if (check(TokenType::KEYWORD) && peek().value == "while") return parseWhile();
    if (check(TokenType::KEYWORD) && peek().value == "for") return parseFor();
    if (check(TokenType::KEYWORD) && peek().value == "return") return parseReturn();
    if (check(TokenType::KEYWORD) && peek().value == "throw") return parseThrow();

    return parseExpressionStatement();
}

ASTNodePtr Parser::parseBlock() {
    Token open = peek();
    consume(TokenType::LEFT_BRACE, "Expected '{' to start block");
    auto block = std::make_unique<BlockStmt>(open.line, open.column);
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto stmt = parseDeclarationOrStatement();
        if (stmt) block->statements.push_back(std::move(stmt));
    }
    consume(TokenType::RIGHT_BRACE, "Expected '}' after block");
    return block;
}

ASTNodePtr Parser::parseExpressionStatement() {
    ASTNodePtr expr = parseExpression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return std::make_unique<ExprStmt>(std::move(expr), peek().line, peek().column);
}

ASTNodePtr Parser::parseFor() {
    Token tk = peek(); advance();
    consume(TokenType::LEFT_PAREN, "Expected '(' after for");

    ASTNodePtr init = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        // Check if it's a variable declaration
        if (isTypeSpecifier() || isTypeQualifier() || isStorageClass()) {
            init = parseVarDeclaration();
            // DEBUG: // std::cerr << "DEBUG parseFor: init parsed as varDecl, next token='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
        } else {
            init = parseExpression();
            // DEBUG: // std::cerr << "DEBUG parseFor: init parsed as expr, next token='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
            consume(TokenType::SEMICOLON, "Expected ';' after for init");
        }
    } else {
        consume(TokenType::SEMICOLON, "Expected ';' after for init (empty)");
    }

    // Support both traditional for and range-based for
    if (check(TokenType::COLON)) {
        // This is a range-based for: 'for ( decl : expr )'
        advance();
        ASTNodePtr rangeExpr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after for range");
        ASTNodePtr body = parseStatement();
        // Represent range-for as a ForStmt with cond/post unused and init holding the decl
        return std::make_unique<ForStmt>(std::move(init), nullptr, std::move(rangeExpr), std::move(body), tk.line, tk.column);
    }

    ASTNodePtr cond = nullptr;
    if (!check(TokenType::SEMICOLON)) cond = parseExpression();
    // DEBUG: // std::cerr << "DEBUG parseFor: after cond, next token='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
    consume(TokenType::SEMICOLON, "Expected ';' after for condition");

    ASTNodePtr post = nullptr;
    if (!check(TokenType::RIGHT_PAREN)) {
        // DEBUG: // std::cerr << "DEBUG parseFor: about to parse post, peek='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
        post = parseExpression();
        // DEBUG: // std::cerr << "DEBUG parseFor: after post, peek='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
    }
    // DEBUG: // std::cerr << "DEBUG parseFor: about to consume RIGHT_PAREN, peek='" << peek().value << "' type:" << static_cast<int>(peek().type) << std::endl;
    consume(TokenType::RIGHT_PAREN, "Expected ')' after for clauses");

    ASTNodePtr body = parseStatement();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(post), std::move(body), tk.line, tk.column);
}

ASTNodePtr Parser::parseIf() {
    Token tk = peek(); advance();
    consume(TokenType::LEFT_PAREN, "Expected '(' after if");
    ASTNodePtr cond = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after if condition");
    ASTNodePtr thenBranch = parseStatement();
    ASTNodePtr elseBranch = nullptr;
    if (check(TokenType::KEYWORD) && peek().value == "else") {
        advance();
        elseBranch = parseStatement();
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch), tk.line, tk.column);
}

ASTNodePtr Parser::parseWhile() {
    Token tk = peek(); advance();
    consume(TokenType::LEFT_PAREN, "Expected '(' after while");
    ASTNodePtr cond = parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ')' after while condition");
    ASTNodePtr body = parseStatement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), tk.line, tk.column);
}

ASTNodePtr Parser::parseReturn() {
    Token tk = peek(); advance();
    ASTNodePtr expr = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        expr = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after return");
    return std::make_unique<ReturnStmt>(std::move(expr), tk.line, tk.column);
}

ASTNodePtr Parser::parseThrow() {
    Token tk = peek(); advance(); // consume 'throw'
    ASTNodePtr expr = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        expr = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after throw");
    // Represent throw as an ExprStmt with the exception expression
    return std::make_unique<ExprStmt>(std::move(expr), tk.line, tk.column);
}

// Expression parsing
// Expression parsing - NEW OPERATOR PRECEDENCE
ASTNodePtr Parser::parseExpression() {
    return parseAssignment();
}

ASTNodePtr Parser::parseAssignment() {
    ASTNodePtr left = parseConditional();  // use conditional at assignment rhs
    if (check(TokenType::OPERATOR) && peek().value == "=") {
        Token op = peek(); advance();
        ASTNodePtr right = parseAssignment();
        return std::make_unique<BinaryOp>(op.value, std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// Conditional (ternary) operator ?:
ASTNodePtr Parser::parseConditional() {
    ASTNodePtr cond = parseLogicalOr();
    if (check(TokenType::OPERATOR) && peek().value == "?") {
        Token q = peek(); advance();
        ASTNodePtr thenExpr = parseExpression();
        consume(TokenType::COLON, "Expected ':' in conditional expression");
        ASTNodePtr elseExpr = parseExpression();
        return std::make_unique<BinaryOp>("?:", std::move(thenExpr), std::move(elseExpr), q.line, q.column);
    }
    return cond;
}

// NEW: Add shift operators (<<, >>)
ASTNodePtr Parser::parseShift() {
    ASTNodePtr node = parseAdditive();
    while ((check(TokenType::LEFT_SHIFT) || check(TokenType::RIGHT_SHIFT)) ||
           (check(TokenType::OPERATOR) && (peek().value == "<<" || peek().value == ">>"))) {
        Token op = peek(); advance();
        ASTNodePtr right = parseAdditive();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}
// NEW: Additive operators (+, -)
ASTNodePtr Parser::parseAdditive() {
    ASTNodePtr node = parseMultiplicative();
    while (check(TokenType::OPERATOR) && (peek().value == "+" || peek().value == "-")) {
        Token op = peek(); advance();
        ASTNodePtr right = parseMultiplicative();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}

// NEW: Multiplicative operators (*, /, %)
ASTNodePtr Parser::parseMultiplicative() {
    ASTNodePtr node = parseUnary();
    while (check(TokenType::OPERATOR) && (peek().value == "*" || peek().value == "/" || peek().value == "%")) {
        Token op = peek(); advance();
        ASTNodePtr right = parseUnary();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}

// Keep your existing parseUnary() method as is

ASTNodePtr Parser::parseLogicalOr() {
    ASTNodePtr node = parseLogicalAnd();
    while (check(TokenType::OPERATOR) && peek().value == "||") {
        Token op = peek(); advance();
        ASTNodePtr right = parseLogicalAnd();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}

ASTNodePtr Parser::parseLogicalAnd() {
    ASTNodePtr node = parseEquality();
    while (check(TokenType::OPERATOR) && peek().value == "&&") {
        Token op = peek(); advance();
        ASTNodePtr right = parseEquality();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}

ASTNodePtr Parser::parseEquality() {
    ASTNodePtr node = parseComparison();
    while (check(TokenType::OPERATOR) && (peek().value == "==" || peek().value == "!=")) {
        Token op = peek(); advance();
        ASTNodePtr right = parseComparison();
        node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
    }
    return node;
}

ASTNodePtr Parser::parseComparison() {
    ASTNodePtr node = parseShift();  // CHANGED: from parseTerm() to parseShift()
    while (true) {
        if (check(TokenType::LESS) || check(TokenType::GREATER) ||
            check(TokenType::LESS_EQUAL) || check(TokenType::GREATER_EQUAL)) {
            Token op = peek(); advance();
            ASTNodePtr right = parseShift();  // CHANGED: from parseTerm() to parseShift()
            node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
        } else if (check(TokenType::OPERATOR) && (peek().value == "<" || peek().value == ">" ||
                   peek().value == "<=" || peek().value == ">=")) {
            Token op = peek(); advance();
            ASTNodePtr right = parseShift();  // CHANGED: from parseTerm() to parseShift()
            node = std::make_unique<BinaryOp>(op.value, std::move(node), std::move(right), op.line, op.column);
        } else {
            break;
        }
    }
    return node;
}



ASTNodePtr Parser::parseUnary() {
    // Handle 'new' operator
    if (check(TokenType::KEYWORD) && peek().value == "new") {
        Token op = peek(); advance();
        
        // Skip type specifier (int, float, etc.)
        if (check(TokenType::TYPE_SPECIFIER)) {
            std::string type_name = peek().value;
            advance();
            
            // Check for array allocation: new int[size]
            if (check(TokenType::LEFT_BRACKET)) {
                advance(); // consume '['
                ASTNodePtr size_expr = parseExpression();
                if (!check(TokenType::RIGHT_BRACKET)) {
                    error(peek(), "Expected ']' after array size");
                }
                advance(); // consume ']'
                
                // Create array subscript node for new operator
                auto dummy_array = std::make_unique<Identifier>(type_name, op.line, op.column);
                return std::make_unique<UnaryOp>("new", 
                    std::make_unique<ArraySubscript>(std::move(dummy_array), std::move(size_expr), op.line, op.column),
                    op.line, op.column);
            }
            
            // Single allocation: new int
            return std::make_unique<UnaryOp>("new", 
                std::make_unique<Identifier>(type_name, op.line, op.column),
                op.line, op.column);
        }
        
        // Fallback to expression
        ASTNodePtr operand = parseUnary();
        return std::make_unique<UnaryOp>("new", std::move(operand), op.line, op.column);
    }
    
    // Handle 'delete' operator
    if (check(TokenType::KEYWORD) && peek().value == "delete") {
        Token op = peek(); advance();
        ASTNodePtr operand = parseUnary();
        return std::make_unique<UnaryOp>("delete", std::move(operand), op.line, op.column);
    }
    
    if (check(TokenType::OPERATOR) && (peek().value == "!" || peek().value == "-" ||
                                       peek().value == "+" || peek().value == "*" ||
                                       peek().value == "&" || peek().value == "~")) {
        Token op = peek(); advance();
        ASTNodePtr operand = parseUnary();
        return std::make_unique<UnaryOp>(op.value, std::move(operand), op.line, op.column);
    }
    return parseCallAndPrimary();
}

// FIXED: parseCallAndPrimary with ARROW, DOT tokens and array subscript support
ASTNodePtr Parser::parseCallAndPrimary() {
    const Token &t = peek();

    // Lambda primary: start with '['
    if (t.type == TokenType::LEFT_BRACKET) {
        Token lb = peek(); advance();
        // consume until matching '}' if present
        while (!check(TokenType::LEFT_BRACE) && !isAtEnd()) advance();
        if (check(TokenType::LEFT_BRACE)) {
            int depth = 0;
            advance(); depth = 1;
            while (!isAtEnd() && depth > 0) {
                if (check(TokenType::LEFT_BRACE)) { advance(); depth++; continue; }
                if (check(TokenType::RIGHT_BRACE)) { advance(); depth--; if (depth==0) break; continue; }
                advance();
            }
        }
        return std::make_unique<Literal>("<lambda>", lb.line, lb.column, TokenType::LEFT_BRACE);
    }

    // Literals
    if (t.type == TokenType::NUMBER || t.type == TokenType::STRING || t.type == TokenType::CHARACTER) {
        TokenType litType = t.type;
        advance();
        return std::make_unique<Literal>(t.value, t.line, t.column, litType);
    }

    // Identifiers
    if (t.type == TokenType::IDENTIFIER) {
        advance();
        ASTNodePtr left = std::make_unique<Identifier>(t.value, t.line, t.column);

        // Handle postfix operators: ->, ., [], (), lambdas
        while (true) {
            // Arrow operator: ->
            if (check(TokenType::ARROW)) {
                Token op = peek(); advance();
                if (!check(TokenType::IDENTIFIER))
                    error(peek(), "Expected member name after '->'");
                Token mem = peek(); advance();
                left = std::make_unique<MemberAccess>(std::move(left), mem.value, true, op.line, op.column);
                continue;
            }
            // Dot operator: .
            else if (check(TokenType::DOT)) {
                Token op = peek(); advance();
                if (!check(TokenType::IDENTIFIER))
                    error(peek(), "Expected member name after '.'");
                Token mem = peek(); advance();
                left = std::make_unique<MemberAccess>(std::move(left), mem.value, false, op.line, op.column);
                continue;
            }
            // Array subscript: []
            else if (check(TokenType::LEFT_BRACKET)) {
                Token bracket = peek(); advance();
                ASTNodePtr index = parseExpression();
                consume(TokenType::RIGHT_BRACKET, "Expected ']' after array index");
                left = std::make_unique<ArraySubscript>(std::move(left), std::move(index), bracket.line, bracket.column);
                continue;
            }
            // Function call: ()
            else if (check(TokenType::LEFT_PAREN)) {
                Token open = peek(); advance();
                std::vector<ASTNodePtr> args;
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        args.push_back(parseExpression());
                    } while (match({TokenType::COMMA}));
                }
                consume(TokenType::RIGHT_PAREN, "Expected ')' after function call arguments");
                left = std::make_unique<CallExpr>(std::move(left), std::move(args), open.line, open.column);
                continue;
            }
            // Lambda literal: []() { ... }
            else if (check(TokenType::LEFT_BRACKET)) {
                // crude lambda support: consume until matching '}' and return as Literal node (placeholder)
                Token lb = peek(); advance();
                // consume until ']' then parameters and body
                while (!check(TokenType::RIGHT_BRACKET) && !isAtEnd()) advance();
                if (check(TokenType::RIGHT_BRACKET)) advance();
                if (check(TokenType::LEFT_PAREN)) {
                    while (!check(TokenType::LEFT_BRACE) && !isAtEnd()) advance();
                }
                if (check(TokenType::LEFT_BRACE)) {
                    Token ob = peek(); advance();
                    std::string contents;
                    int depth = 1;
                    while (!isAtEnd() && depth > 0) {
                        if (check(TokenType::LEFT_BRACE)) { contents += "{"; advance(); depth++; continue; }
                        if (check(TokenType::RIGHT_BRACE)) { contents += "}"; advance(); depth--; if (depth==0) break; continue; }
                        contents += peek().value;
                        advance();
                    }
                    left = std::make_unique<Literal>("<lambda>", lb.line, lb.column, TokenType::LEFT_BRACE);
                    continue;
                }
            }
            // Postfix increment/decrement: ++ --
            else if (check(TokenType::OPERATOR) && (peek().value == "++" || peek().value == "--")) {
                Token op = peek(); advance();
                // Represent postfix as UnaryOp with '_post' suffix
                left = std::make_unique<UnaryOp>(op.value + "_post", std::move(left), op.line, op.column);
                continue;
            }
            // Scope resolution: ::
            else if (check(TokenType::SCOPE_RESOLUTION)) {
                // For now, just consume it and get the next identifier
                // Full qualified name support would require a new AST node
                advance(); // consume ::
                if (check(TokenType::IDENTIFIER)) {
                    Token nextId = peek(); advance();
                    // For simplicity, create a new identifier with qualified name
                    std::string qualifiedName = dynamic_cast<Identifier*>(left.get())->name + "::" + nextId.value;
                    left = std::make_unique<Identifier>(qualifiedName, nextId.line, nextId.column);
                    continue;
                }
            }
            else {
                break;
            }
        }

        return left;
    }

    // Parenthesized expressions
    if (check(TokenType::LEFT_PAREN)) {
        Token open = peek(); advance();
        ASTNodePtr expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }

    error(t, "Expected expression");
    return nullptr;
}
