#ifndef DS_PROJECT_GREY0NE_DEV_PARSER_H
#define DS_PROJECT_GREY0NE_DEV_PARSER_H

#include "lexer.h"
#include <memory>
#include <vector>
#include <string>
#include <iostream>

// Forward declarations
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

enum class ASTNodeKind {
    PROGRAM,
    BLOCK,
    VAR_DECL,
    FUNC_DECL,
    RETURN,
    IF,
    WHILE,
    FOR,
    EXPR_STMT,
    BINARY_OP,
    UNARY_OP,
    LITERAL,
    IDENTIFIER,
    CALL,
    MEMBER_ACCESS,
    ARRAY_SUBSCRIPT,  // NEW: for arr[index]

    // C++ specific
    CLASS_DECL,
    STRUCT_DECL,
    NAMESPACE_DECL,
    TEMPLATE_DECL,
    ACCESS_SPEC,
    INCLUDE_DIRECTIVE,
    USING_DIRECTIVE,
    CONSTRUCTOR,
    DESTRUCTOR,
    FRIEND_DECL,
    OPERATOR_OVERLOAD,
    TEMPLATE_PARAM
};

// Base AST node
struct ASTNode {
    ASTNodeKind kind;
    int line;
    int column;
    ASTNode(ASTNodeKind k, int l = 0, int c = 0) : kind(k), line(l), column(c) {}
    virtual ~ASTNode() = default;
    virtual void dump(int indent = 0) const = 0;
};

// Helpers
std::string indentStr(int n);

// Specific nodes
struct Expr : ASTNode {
    Expr(ASTNodeKind k, int l=0, int c=0) : ASTNode(k,l,c) {}
};

struct Statement : ASTNode {
    Statement(ASTNodeKind k, int l=0, int c=0) : ASTNode(k,l,c) {}
};

struct Declaration : ASTNode {
    Declaration(ASTNodeKind k, int l=0, int c=0) : ASTNode(k,l,c) {}
};

// Literal
struct Literal : Expr {
    std::string value;
    TokenType litType;  // Track whether it's NUMBER, STRING, or CHARACTER
    Literal(const std::string &v, int l, int c, TokenType tt = TokenType::NUMBER) 
        : Expr(ASTNodeKind::LITERAL, l, c), value(v), litType(tt) {}
    void dump(int indent = 0) const override;
};

// Identifier
struct Identifier : Expr {
    std::string name;
    Identifier(const std::string &n, int l, int c) : Expr(ASTNodeKind::IDENTIFIER, l, c), name(n) {}
    void dump(int indent = 0) const override;
};

struct UnaryOp : Expr {
    std::string op;
    ASTNodePtr operand;
    UnaryOp(const std::string &o, ASTNodePtr opd, int l, int c) : Expr(ASTNodeKind::UNARY_OP, l, c), op(o), operand(std::move(opd)) {}
    void dump(int indent = 0) const override;
};

struct BinaryOp : Expr {
    std::string op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOp(const std::string &o, ASTNodePtr lft, ASTNodePtr rgt, int ln, int col) : Expr(ASTNodeKind::BINARY_OP, ln, col), op(o), left(std::move(lft)), right(std::move(rgt)) {}
    void dump(int indent = 0) const override;
};

struct CallExpr : Expr {
    ASTNodePtr callee;
    std::vector<ASTNodePtr> args;
    CallExpr(ASTNodePtr cal, std::vector<ASTNodePtr> a, int l, int c) : Expr(ASTNodeKind::CALL, l, c), callee(std::move(cal)), args(std::move(a)) {}
    void dump(int indent = 0) const override;
};

struct MemberAccess : Expr {
    ASTNodePtr object;
    std::string member;
    bool arrow; // -> vs .
    MemberAccess(ASTNodePtr obj, std::string m, bool ar, int l, int c) : Expr(ASTNodeKind::MEMBER_ACCESS, l, c), object(std::move(obj)), member(std::move(m)), arrow(ar) {}
    void dump(int indent = 0) const override;
};

// NEW: Array subscript node
struct ArraySubscript : Expr {
    ASTNodePtr array;
    ASTNodePtr index;
    ArraySubscript(ASTNodePtr arr, ASTNodePtr idx, int l, int c)
        : Expr(ASTNodeKind::ARRAY_SUBSCRIPT, l, c), array(std::move(arr)), index(std::move(idx)) {}
    void dump(int indent = 0) const override;
};

// Statements & program
struct ExprStmt : Statement {
    ASTNodePtr expr;
    ExprStmt(ASTNodePtr e, int l, int c) : Statement(ASTNodeKind::EXPR_STMT, l, c), expr(std::move(e)) {}
    void dump(int indent = 0) const override;
};

struct VarDecl : Statement {
    std::vector<std::string> typeTokens;  // CHANGED: was std::string typeName
    std::string varName;
    ASTNodePtr init; // optional
    bool isPointer;  // NEW: track if it's a pointer
    bool isReference; // NEW: track if it's a reference
    bool isArray;    // NEW: track if it's an array declaration
    VarDecl(std::vector<std::string> t, std::string n, ASTNodePtr i, int l, int c)
        : Statement(ASTNodeKind::VAR_DECL, l, c), typeTokens(std::move(t)), varName(std::move(n)),
          init(std::move(i)), isPointer(false), isReference(false), isArray(false) {}
    void dump(int indent = 0) const override;
};

struct BlockStmt : Statement {
    std::vector<ASTNodePtr> statements;
    BlockStmt(int l, int c) : Statement(ASTNodeKind::BLOCK, l, c) {}
    void dump(int indent = 0) const override;
};

struct IfStmt : Statement {
    ASTNodePtr cond;
    ASTNodePtr thenBranch;
    ASTNodePtr elseBranch; // optional
    IfStmt(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e, int l, int ccol) : Statement(ASTNodeKind::IF, l, ccol), cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    void dump(int indent = 0) const override;
};

struct WhileStmt : Statement {
    ASTNodePtr cond;
    ASTNodePtr body;
    WhileStmt(ASTNodePtr c, ASTNodePtr b, int l, int ccol) : Statement(ASTNodeKind::WHILE, l, ccol), cond(std::move(c)), body(std::move(b)) {}
    void dump(int indent = 0) const override;
};

struct ForStmt : Statement {
    ASTNodePtr init; // vardecl or exprstmt or nullptr
    ASTNodePtr cond; // expr or nullptr
    ASTNodePtr post; // expr or nullptr
    ASTNodePtr body;
    ForStmt(ASTNodePtr i, ASTNodePtr c, ASTNodePtr p, ASTNodePtr b, int l, int ccol) : Statement(ASTNodeKind::FOR, l, ccol), init(std::move(i)), cond(std::move(c)), post(std::move(p)), body(std::move(b)) {}
    void dump(int indent = 0) const override;
};

struct ReturnStmt : Statement {
    ASTNodePtr expr; // optional
    ReturnStmt(ASTNodePtr e, int l, int c) : Statement(ASTNodeKind::RETURN, l, c), expr(std::move(e)) {}
    void dump(int indent = 0) const override;
};

// C++ specific nodes
struct ClassDecl : Declaration {
    std::string className;
    std::vector<ASTNodePtr> members;
    std::vector<std::string> baseClasses;  // CHANGED: simplified from ASTNodePtr
    ClassDecl(const std::string& name, int l, int c) : Declaration(ASTNodeKind::CLASS_DECL, l, c), className(name) {}
    void dump(int indent = 0) const override;
};

struct StructDecl : Declaration {
    std::string structName;
    std::vector<ASTNodePtr> members;
    StructDecl(const std::string& name, int l, int c) : Declaration(ASTNodeKind::STRUCT_DECL, l, c), structName(name) {}
    void dump(int indent = 0) const override;
};

struct NamespaceDecl : Declaration {
    std::string name;
    ASTNodePtr body;
    NamespaceDecl(const std::string& n, ASTNodePtr b, int l, int c) : Declaration(ASTNodeKind::NAMESPACE_DECL, l, c), name(n), body(std::move(b)) {}
    void dump(int indent = 0) const override;
};

struct TemplateDecl : Declaration {
    std::vector<std::string> params;
    ASTNodePtr declaration;
    TemplateDecl(std::vector<std::string> p, ASTNodePtr decl, int l, int c) : Declaration(ASTNodeKind::TEMPLATE_DECL, l, c), params(std::move(p)), declaration(std::move(decl)) {}
    void dump(int indent = 0) const override;
};

struct AccessSpec : Statement {
    std::string access; // "public", "private", "protected"
    AccessSpec(const std::string& acc, int l, int c) : Statement(ASTNodeKind::ACCESS_SPEC, l, c), access(acc) {}
    void dump(int indent = 0) const override;
};

struct IncludeDirective : Statement {
    std::string file;
    bool isSystem;
    IncludeDirective(const std::string& f, bool sys, int l, int c) : Statement(ASTNodeKind::INCLUDE_DIRECTIVE, l, c), file(f), isSystem(sys) {}
    void dump(int indent = 0) const override;
};

struct UsingDirective : Statement {
    std::string namespaceName;
    UsingDirective(const std::string& name, int l, int c) : Statement(ASTNodeKind::USING_DIRECTIVE, l, c), namespaceName(name) {}
    void dump(int indent = 0) const override;
};

struct FunctionDecl : Declaration {
    std::vector<std::string> returnTypeTokens;  // CHANGED: better type representation
    std::string funcName;
    std::vector<std::pair<std::vector<std::string>, std::string>> params; // CHANGED: type tokens, name
    ASTNodePtr body;
    bool isVirtual;
    bool isConst;  // NEW: for const member functions
    FunctionDecl(const std::vector<std::string>& ret, const std::string& name,
                 std::vector<std::pair<std::vector<std::string>, std::string>> p, ASTNodePtr b, int l, int c)
        : Declaration(ASTNodeKind::FUNC_DECL, l, c), returnTypeTokens(ret), funcName(name),
          params(std::move(p)), body(std::move(b)), isVirtual(false), isConst(false) {}
    void dump(int indent = 0) const override;
};

struct Program {
    std::vector<ASTNodePtr> top;
    void dump() const;
};

// Parser
class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    Program parseProgram();

private:
    const std::vector<Token>& tokens;
    size_t idx;
    std::string currentClassName; // for constructor parsing

    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType t) const;
    bool match(std::initializer_list<TokenType> types);
    void consume(TokenType t, const std::string &msg);

    // NEW: Helper methods for better parsing
    bool isTypeSpecifier() const;
    bool isTypeQualifier() const;
    bool isStorageClass() const;
    std::vector<std::string> parseType();  // NEW: parse complex types
    std::vector<std::string> parseTypeForLookahead(size_t &pos) const;

    // Parsing primitives
    ASTNodePtr parseStatement();
    ASTNodePtr parseBlock();
    ASTNodePtr parseDeclarationOrStatement();
    ASTNodePtr parseVarDeclaration();
    ASTNodePtr parseExpressionStatement();
    ASTNodePtr parseIf();
    ASTNodePtr parseWhile();
    ASTNodePtr parseFor();
    ASTNodePtr parseReturn();
    ASTNodePtr parseThrow();

    // C++ specific parsing
    ASTNodePtr parseClass();
    ASTNodePtr parseStruct();
    ASTNodePtr parseNamespace();
    ASTNodePtr parseTemplate();
    ASTNodePtr parseFunctionDeclaration();
    ASTNodePtr parseAccessSpecifier();
    ASTNodePtr parseIncludeDirective();
    ASTNodePtr parseUsingDirective();
    ASTNodePtr parseClassMember();

    ASTNodePtr parseExpression();
    ASTNodePtr parseAssignment();
    ASTNodePtr parseConditional();
    ASTNodePtr parseLogicalOr();
    ASTNodePtr parseLogicalAnd();
    ASTNodePtr parseEquality();
    ASTNodePtr parseComparison();
    ASTNodePtr parseTerm();
    ASTNodePtr parseFactor();
    ASTNodePtr parseUnary();
    ASTNodePtr parseCallAndPrimary();

    ASTNodePtr parseShift();
    ASTNodePtr parseAdditive();
    ASTNodePtr parseMultiplicative();

    std::vector<std::pair<std::vector<std::string>, std::string>> parseFunctionParams();  // CHANGED signature
    std::vector<std::string> parseTemplateParams();

    bool isAtEnd() const;
    void error(const Token& tok, const std::string& message) const;
};

#endif //DS_PROJECT_GREY0NE_DEV_PARSER_H
