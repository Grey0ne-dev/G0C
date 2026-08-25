#include "codegen.h"
#include "logger.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <stdexcept>
CodeGenerator::CodeGenerator() 
    : current_offset(0), next_memory_addr(0), label_counter(0) {
}

std::vector<uint8_t> CodeGenerator::generate(const Program& program) {
    bytecode.clear();
    symbols.clear();
    functions.clear();
    labels.clear();
    class_names.clear();
    string_table.clear();
    scope_changes.clear();
    current_offset = 0;
    next_memory_addr = 0;
    label_counter = 0;
    
    collectFunctionSignatures(program);
    genProgram(program);
    fixupLabels();
    
    return bytecode;
}

void CodeGenerator::genProgram(const Program& prog) {
    // Collect class/struct names for constructor detection
    for (const auto& node : prog.top) {
        if (!node) continue;  // Safety check
        if (node->kind == ASTNodeKind::CLASS_DECL) {
            auto cls = static_cast<ClassDecl*>(node.get());
            if (cls) class_names.insert(cls->className);
        } else if (node->kind == ASTNodeKind::STRUCT_DECL) {
            auto strct = static_cast<StructDecl*>(node.get());
            if (strct) class_names.insert(strct->structName);
        }
    }

    // Emit global initialization before entering main. Function bodies are
    // emitted afterward so they are not executed during startup.
    for (const auto& node : prog.top) {
        if (!node) continue;
        if (node->kind != ASTNodeKind::FUNC_DECL &&
            node->kind != ASTNodeKind::CLASS_DECL &&
            node->kind != ASTNodeKind::STRUCT_DECL) {
            genStatement(node.get());
        }
    }

    emitJump(Opcode::CALL, "main");
    emit(Opcode::HALT);

    // Generate functions after the startup section.
    for (const auto& node : prog.top) {
        if (!node) continue;
        if (node->kind == ASTNodeKind::CLASS_DECL) {
            auto cls = static_cast<const ClassDecl*>(node.get());
            for (const auto& m : cls->members) {
                if (m && m->kind == ASTNodeKind::FUNC_DECL) {
                    auto f = static_cast<const FunctionDecl*>(m.get());
                    std::string qual = mangleFunctionName(
                        cls->className + "::" + f->funcName, f->params);
                    genFunctionDecl(f, qual);
                }
            }
        } else if (node->kind == ASTNodeKind::FUNC_DECL) {
            genFunctionDecl(static_cast<const FunctionDecl*>(node.get()));
        }
    }
}

void CodeGenerator::collectFunctionSignatures(const Program& prog) {
    for (const auto& node : prog.top) {
        if (!node) continue;
        if (node->kind == ASTNodeKind::FUNC_DECL) {
            auto func = static_cast<const FunctionDecl*>(node.get());
            collectFunctionSignature(func, mangleFunctionName(func->funcName, func->params));
        } else if (node->kind == ASTNodeKind::CLASS_DECL) {
            auto cls = static_cast<const ClassDecl*>(node.get());
            for (const auto& member : cls->members) {
                if (member && member->kind == ASTNodeKind::FUNC_DECL) {
                    auto func = static_cast<const FunctionDecl*>(member.get());
                    collectFunctionSignature(func, mangleFunctionName(
                        cls->className + "::" + func->funcName, func->params));
                }
            }
        }
    }
}

void CodeGenerator::collectFunctionSignature(const FunctionDecl* func, const std::string& nameOverride) {
    if (!func) return;
    FunctionSignature sig;
    sig.returns_float = isFloatType(func->returnTypeTokens);
    for (const auto& param : func->params) {
        sig.params_float.push_back(isFloatType(param.first));
    }
    functions[nameOverride] = sig;
}

void CodeGenerator::genStatement(const ASTNode* node) {
    if (!node) return;
    
    switch (node->kind) {
        case ASTNodeKind::VAR_DECL:
            genVarDecl(static_cast<const VarDecl*>(node));
            break;
        case ASTNodeKind::FUNC_DECL:
            genFunctionDecl(static_cast<const FunctionDecl*>(node));
            break;
        case ASTNodeKind::BLOCK:
            genBlock(static_cast<const BlockStmt*>(node));
            break;
        case ASTNodeKind::IF:
            genIf(static_cast<const IfStmt*>(node));
            break;
        case ASTNodeKind::WHILE:
            genWhile(static_cast<const WhileStmt*>(node));
            break;
        case ASTNodeKind::FOR:
            genFor(static_cast<const ForStmt*>(node));
            break;
        case ASTNodeKind::RETURN:
            genReturn(static_cast<const ReturnStmt*>(node));
            break;
        case ASTNodeKind::EXPR_STMT: {
            auto expr = static_cast<const ExprStmt*>(node);
            if (expr->expr) {
                genExpression(expr->expr.get());
                // Float expressions leave result on FPU; int expressions on int stack
                if (isFloatExpr(expr->expr.get())) {
                    emit(Opcode::FPOP);
                } else {
                    emit(Opcode::POP); // Discard expression result
                }
            }
            break;
        }
        // C++ specific nodes - skip for now (no codegen needed)
        case ASTNodeKind::CLASS_DECL:
        case ASTNodeKind::STRUCT_DECL:
        case ASTNodeKind::NAMESPACE_DECL:
        case ASTNodeKind::TEMPLATE_DECL:
        case ASTNodeKind::ACCESS_SPEC:
        case ASTNodeKind::INCLUDE_DIRECTIVE:
        case ASTNodeKind::USING_DIRECTIVE:
            // These don't generate runtime code for now
            // Just skip them silently
            break;
        default:
            Logger::error() << "Warning: Unhandled statement type "
                      << static_cast<int>(node->kind) << " in codegen\n";
            break;
    }
}

void CodeGenerator::genVarDecl(const VarDecl* decl) {
    // Check if this is a pointer type explicitly
    bool is_pointer = decl->isPointer;
    for (const auto& token : decl->typeTokens) {
        if (token == "*") {
            is_pointer = true;
            break;
        }
    }
    
    // Check if this is an array OR pointer to heap-allocated array
    // Detect "new" expressions in initializer (heap allocation)
    bool is_heap_array = false;
    if (is_pointer && decl->init) {
        // Check if initializer is a "new" expression (heap allocation)
        if (decl->init->kind == ASTNodeKind::UNARY_OP) {
            auto unop = static_cast<const UnaryOp*>(decl->init.get());
            if (unop->op == "new") {
                is_heap_array = true;
            }
        }
    }
    
    bool is_array = decl->isArray || is_heap_array;
    
    // Detect float/double variable type (non-pointer, non-array)
    bool is_float_var = !is_pointer && !is_array && isFloatType(decl->typeTokens);
    
    // Allocate memory address for this variable. Stack arrays reserve one cell
    // per element; heap arrays only reserve one cell for the heap pointer.
    int allocation_size = (decl->isArray && !is_heap_array) ? decl->arraySize : 1;
    int addr = next_memory_addr;
    next_memory_addr += allocation_size;
    addVariable(decl->varName, addr, is_array, is_heap_array, is_float_var);
    
    // If there's an initializer, evaluate it and store
    if (decl->init) {
        if (decl->init->kind == ASTNodeKind::INITIALIZER_LIST) {
            auto list = static_cast<const InitializerList*>(decl->init.get());
            if (!decl->isArray) {
                if (list->elements.size() != 1) {
                    throw std::runtime_error("Scalar initializer for '" + decl->varName +
                                             "' must contain exactly one value");
                }
                genExpression(list->elements.front().get());
                if (is_float_var) {
                    if (!isFloatExpr(list->elements.front().get())) emit(Opcode::INT_TO_FP);
                    emit(Opcode::FSTORE);
                    emitInt32(addr);
                } else {
                    emit(Opcode::PUSH);
                    emitInt32(addr);
                    emit(Opcode::STORE);
                }
                return;
            }
            if (list->elements.size() > static_cast<size_t>(allocation_size)) {
                throw std::runtime_error("Too many initializers for array '" + decl->varName + "'");
            }
            for (size_t i = 0; i < list->elements.size(); ++i) {
                if (isFloatExpr(list->elements[i].get())) {
                    throw std::runtime_error("Float arrays are not supported by the current VM");
                }
                genExpression(list->elements[i].get());
                emit(Opcode::PUSH);
                emitInt32(addr + static_cast<int>(i));
                emit(Opcode::STORE);
            }
            return;
        }
        if (is_float_var) {
            // Float variable: generate expression, coerce if needed, FSTORE
            genExpression(decl->init.get());
            if (!isFloatExpr(decl->init.get())) {
                emit(Opcode::INT_TO_FP);
            }
            emit(Opcode::FSTORE);
            emitInt32(addr);
        } else {
            genExpression(decl->init.get());
            emit(Opcode::PUSH);
            emitInt32(addr);
            emit(Opcode::STORE);
        }
    }
}

void CodeGenerator::genFunctionDecl(const FunctionDecl* func) {
    std::string mangled_name = mangleFunctionName(func->funcName, func->params);
    genFunctionDecl(func, mangled_name);
}

void CodeGenerator::genFunctionDecl(const FunctionDecl* func, const std::string& nameOverride) {
    // Define function label
    Logger::debug() << "DBG genFunctionDecl: defining label '" << nameOverride
                    << "' at address " << currentAddress() << std::endl;
    defineLabel(nameOverride);
    
    // Function prologue
    emit(Opcode::PUSH_BP);
    enterScope();
    
    // Parameters are on stack below saved BP
    // After PUSH_BP, stack layout:
    // [... caller stuff, argN, ..., arg2, arg1, saved_BP] <- BP points here.
    // cdecl-style callers push right-to-left, so arg1 is BP-2.
    int param_count = func->params.size();
    for (int i = 0; i < param_count; i++) {
        int offset = -(i + 2);
        
        // Check if parameter is a pointer/array
        bool is_pointer = false;
        const auto& type_tokens = func->params[i].first;
        for (const auto& token : type_tokens) {
            if (token == "*" || token == "[]") {
                is_pointer = true;
                break;
            }
        }
        
        Symbol sym;
        sym.type = Symbol::PARAMETER;
        sym.offset = offset;
        sym.address = 0;
        sym.param_count = 0;
        sym.is_array = is_pointer;  // Pointers and arrays treated same
        sym.is_heap_allocated = false;
        sym.is_float = !is_pointer && isFloatType(type_tokens);
        rememberSymbolBeforeChange(func->params[i].second);
        symbols[func->params[i].second] = sym;
        
        Logger::debug() << "DBG addParam: '" << func->params[i].second
                        << "' offset=" << offset << " is_array=" << is_pointer
                        << " is_float=" << sym.is_float << std::endl;
    }
    
    // Generate function body
    if (func->body) {
        genStatement(func->body.get());
    }
    exitScope();
    
    // Function epilogue (if no explicit return)
    emit(Opcode::POP_BP);
    emit(Opcode::RET);
}

void CodeGenerator::genBlock(const BlockStmt* block) {
    enterScope();
    for (const auto& stmt : block->statements) {
        genStatement(stmt.get());
    }
    exitScope();
}

void CodeGenerator::genIf(const IfStmt* ifstmt) {
    std::string else_label = makeLabel("else");
    std::string end_label = makeLabel("endif");
    
    // Evaluate condition
    genCondition(ifstmt->cond.get());
    
    // Jump to else if condition is zero (false)
    emitJump(Opcode::JZ, else_label);
    
    // Then branch
    genStatement(ifstmt->thenBranch.get());
    emitJump(Opcode::JMP, end_label);
    
    // Else branch
    defineLabel(else_label);
    if (ifstmt->elseBranch) {
        genStatement(ifstmt->elseBranch.get());
    }
    
    defineLabel(end_label);
}

void CodeGenerator::genWhile(const WhileStmt* whilestmt) {
    std::string loop_start = makeLabel("while_start");
    std::string loop_end = makeLabel("while_end");
    
    defineLabel(loop_start);
    
    // Evaluate condition
    genCondition(whilestmt->cond.get());
    
    // Exit loop if condition is false
    emitJump(Opcode::JZ, loop_end);
    
    // Loop body
    genStatement(whilestmt->body.get());
    
    // Jump back to start
    emitJump(Opcode::JMP, loop_start);
    
    defineLabel(loop_end);
}

void CodeGenerator::genFor(const ForStmt* forstmt) {
    std::string loop_start = makeLabel("for_start");
    std::string loop_end = makeLabel("for_end");
    
    // Initialization
    if (forstmt->init) {
        genStatement(forstmt->init.get());
    }
    
    defineLabel(loop_start);
    
    // Condition
    if (forstmt->cond) {
        genCondition(forstmt->cond.get());
        emitJump(Opcode::JZ, loop_end);
    }
    
    // Body
    genStatement(forstmt->body.get());
    
    // Post-expression
    if (forstmt->post) {
        genExpression(forstmt->post.get());
        if (isFloatExpr(forstmt->post.get())) emit(Opcode::FPOP);
        else emit(Opcode::POP); // Discard result
    }
    
    emitJump(Opcode::JMP, loop_start);
    defineLabel(loop_end);
}

void CodeGenerator::genReturn(const ReturnStmt* ret) {
    if (ret->expr) {
        genExpression(ret->expr.get());
    }
    emit(Opcode::POP_BP);
    emit(Opcode::RET);
}

void CodeGenerator::genCondition(const ASTNode* node) {
    if (!isFloatExpr(node)) {
        genExpression(node);
        return;
    }

    genExpression(node);
    emit(Opcode::FPUSH);
    emitFloat32(0.0f);
    emit(Opcode::FCMP);
    std::string true_label = makeLabel("float_truthy");
    std::string end_label = makeLabel("float_truthy_end");
    emitJump(Opcode::JL, true_label);
    emitJump(Opcode::JG, true_label);
    emit(Opcode::PUSH);
    emitInt32(0);
    emitJump(Opcode::JMP, end_label);
    defineLabel(true_label);
    emit(Opcode::PUSH);
    emitInt32(1);
    defineLabel(end_label);
}

void CodeGenerator::genConditional(const ConditionalExpr* conditional) {
    const bool result_is_float = isFloatExpr(conditional->thenExpr.get()) ||
                                 isFloatExpr(conditional->elseExpr.get());
    std::string else_label = makeLabel("conditional_else");
    std::string end_label = makeLabel("conditional_end");
    genCondition(conditional->condition.get());
    emitJump(Opcode::JZ, else_label);
    genExpression(conditional->thenExpr.get());
    if (result_is_float && !isFloatExpr(conditional->thenExpr.get())) emit(Opcode::INT_TO_FP);
    emitJump(Opcode::JMP, end_label);
    defineLabel(else_label);
    genExpression(conditional->elseExpr.get());
    if (result_is_float && !isFloatExpr(conditional->elseExpr.get())) emit(Opcode::INT_TO_FP);
    defineLabel(end_label);
}

void CodeGenerator::genExpression(const ASTNode* node) {
    if (!node) return;
    
    switch (node->kind) {
        case ASTNodeKind::BINARY_OP:
            genBinaryOp(static_cast<const BinaryOp*>(node));
            break;
        case ASTNodeKind::UNARY_OP:
            genUnaryOp(static_cast<const UnaryOp*>(node));
            break;
        case ASTNodeKind::CALL:
            genCall(static_cast<const CallExpr*>(node));
            break;
        case ASTNodeKind::LITERAL:
            genLiteral(static_cast<const Literal*>(node));
            break;
        case ASTNodeKind::IDENTIFIER:
            genIdentifier(static_cast<const Identifier*>(node));
            break;
        case ASTNodeKind::ARRAY_SUBSCRIPT:
            genArraySubscript(static_cast<const ArraySubscript*>(node));
            break;
        case ASTNodeKind::CONDITIONAL:
            genConditional(static_cast<const ConditionalExpr*>(node));
            break;
        case ASTNodeKind::INITIALIZER_LIST:
            throw std::runtime_error("Initializer list is only valid in a declaration");
        case ASTNodeKind::MEMBER_ACCESS:
            throw std::runtime_error("Member access is not supported by code generation");
        default:
            throw std::runtime_error("Unhandled expression type in code generation");
    }
}

void CodeGenerator::genBinaryOp(const BinaryOp* binop) {
    // Special handling for assignment
    if (binop->op == "=") {
        // Handle pointer dereference assignment: *ptr = value
        if (binop->left->kind == ASTNodeKind::UNARY_OP) {
            auto unop = static_cast<const UnaryOp*>(binop->left.get());
            if (unop->op == "*") {
                // Dereference on left side of assignment
                // Evaluate right side first
                genExpression(binop->right.get());
                emit(Opcode::DUP); // Keep value for result
                
                // Get address from pointer variable
                genExpression(unop->operand.get()); // Push pointer value (which is an address)
                
                // Stack: [value, value, addr]
                emit(Opcode::STORE_INDIRECT);
                return;
            }
        }
        
        // Handle array subscript assignment
        if (binop->left->kind == ASTNodeKind::ARRAY_SUBSCRIPT) {
            auto sub = static_cast<const ArraySubscript*>(binop->left.get());
            
            // Evaluate right side first
            genExpression(binop->right.get());
            emit(Opcode::DUP); // Keep value for result
            
            // Calculate array element address
            if (sub->array->kind == ASTNodeKind::IDENTIFIER) {
                auto id = static_cast<const Identifier*>(sub->array.get());
                auto sym = findSymbol(id->name);
                
                if (sym) {
                    // Push base address
                    if (sym->type == Symbol::PARAMETER && sym->is_array) {
                        emit(Opcode::LOAD_BP);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_heap_allocated) {
                        // Heap arrays: load the heap pointer
                        emit(Opcode::LOAD);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_array) {
                        // Stack arrays: use the stack address
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    } else {
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    }
                    genExpression(sub->index.get());
                    emit(Opcode::ADD);
                    // Stack: [value, value, addr]
                    // STORE_INDIRECT pops addr, pops value
                    emit(Opcode::STORE_INDIRECT);
                } else {
                    throw std::runtime_error("Unknown array '" + id->name + "'");
                }
            } else {
                throw std::runtime_error("Unsupported array assignment target");
            }
            return;
        }
        
        // Left side must be identifier
        if (binop->left->kind == ASTNodeKind::IDENTIFIER) {
            auto id = static_cast<const Identifier*>(binop->left.get());
            auto sym = findSymbol(id->name);
            
            // Evaluate right side - leaves value on stack
            genExpression(binop->right.get());
            
            if (sym) {
                if (sym->is_float) {
                    // Float variable assignment
                    if (!isFloatExpr(binop->right.get())) {
                        emit(Opcode::INT_TO_FP);
                    }
                    emit(Opcode::FDUP); // keep copy for expression result
                    if (sym->type == Symbol::PARAMETER) {
                        emit(Opcode::FP_TO_BITS);
                        emit(Opcode::STORE_BP);
                        emitInt32(sym->offset);
                    } else {
                        emit(Opcode::FSTORE);
                        emitInt32(sym->offset);
                    }
                } else if (sym->type == Symbol::PARAMETER) {
                    // Parameters use BP-relative addressing
                    emit(Opcode::DUP); // Keep value for expression result
                    emit(Opcode::STORE_BP);
                    emitInt32(sym->offset);
                } else {
                    // Variables use absolute addressing
                    // Stack: [value]
                    emit(Opcode::DUP); // Stack: [value, value]
                    emit(Opcode::PUSH);
                    emitInt32(sym->offset); // Stack: [value, value, addr]
                    emit(Opcode::STORE);
                }
            } else {
                throw std::runtime_error("Assignment to unknown identifier '" + id->name + "'");
            }
        } else {
            throw std::runtime_error("Unsupported assignment target at line " +
                                     std::to_string(binop->line));
        }
        return;
    }

    if (binop->op == "&&" || binop->op == "||") {
        std::string decisive_label = makeLabel(binop->op == "&&" ? "and_false" : "or_true");
        std::string end_label = makeLabel("logical_end");
        genCondition(binop->left.get());
        emitJump(binop->op == "&&" ? Opcode::JZ : Opcode::JNZ, decisive_label);
        genCondition(binop->right.get());
        emitJump(binop->op == "&&" ? Opcode::JZ : Opcode::JNZ, decisive_label);
        emit(Opcode::PUSH);
        emitInt32(binop->op == "&&" ? 1 : 0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(decisive_label);
        emit(Opcode::PUSH);
        emitInt32(binop->op == "&&" ? 0 : 1);
        defineLabel(end_label);
        return;
    }
    
    // Handle << operator (stream output operator)
    if (binop->op == "<<") {
        // Special-case: chained cout << a << b << endl
        // If left is identifier std::cout or a chain starting from it, handle print
        bool isCoutChain = false;
        // Walk leftmost to see if base is std::cout
        const ASTNode* leftmost = binop->left.get();
        while (leftmost && leftmost->kind == ASTNodeKind::BINARY_OP) {
            leftmost = static_cast<const BinaryOp*>(leftmost)->left.get();
        }
        if (leftmost && leftmost->kind == ASTNodeKind::IDENTIFIER) {
            auto id = static_cast<const Identifier*>(leftmost);
            if (id->name == "std::cout") isCoutChain = true;
        }

        if (isCoutChain) {
            // If left is another << chain, process it first so earlier parts are printed
            if (binop->left->kind == ASTNodeKind::BINARY_OP) {
                genBinaryOp(static_cast<const BinaryOp*>(binop->left.get()));
                emit(Opcode::POP); // discard the previous chain placeholder
            }
            // For chained prints, print right side then return
            if (binop->right->kind == ASTNodeKind::LITERAL) {
                auto lit = static_cast<const Literal*>(binop->right.get());
                if (lit->litType == TokenType::STRING) {
                    int str_id = addString(lit->value);
                    emit(Opcode::PUSH_STR);
                    emitInt32(str_id);
                    emit(Opcode::PRINT_STR);
                } else {
                    genExpression(binop->right.get());
                    if (isFloatExpr(binop->right.get())) emit(Opcode::FPRINT);
                    else emit(Opcode::PRINT);
                }
            } else if (binop->right->kind == ASTNodeKind::IDENTIFIER) {
                auto id = static_cast<const Identifier*>(binop->right.get());
                if (id->name == "std::endl" || id->name == "endl") {
                    int str_id = addString("\n");
                    emit(Opcode::PUSH_STR);
                    emitInt32(str_id);
                    emit(Opcode::PRINT_STR);
                } else {
                    genExpression(binop->right.get());
                    if (isFloatExpr(binop->right.get())) emit(Opcode::FPRINT);
                    else emit(Opcode::PRINT);
                }
            } else {
                genExpression(binop->right.get());
                if (isFloatExpr(binop->right.get())) emit(Opcode::FPRINT);
                else emit(Opcode::PRINT);
            }
            // Push dummy for chaining
            emit(Opcode::PUSH);
            emitInt32(0);
            return;
        }

        // Otherwise default behavior
        if (binop->right->kind == ASTNodeKind::LITERAL) {
            auto lit = static_cast<const Literal*>(binop->right.get());
            if (lit->litType == TokenType::STRING) {
                int str_id = addString(lit->value);
                emit(Opcode::PUSH_STR);
                emitInt32(str_id);
                emit(Opcode::PRINT_STR);
                emit(Opcode::PUSH);
                emitInt32(0);
                return;
            }
        }
        genExpression(binop->right.get());
        if (isFloatExpr(binop->right.get())) emit(Opcode::FPRINT);
        else emit(Opcode::PRINT);
        emit(Opcode::PUSH);
        emitInt32(0);
        return;
    }
    
    // Handle >> operator (stream input operator) 
    if (binop->op == ">>") {
        // For cin >> variable: input to right side variable
        emit(Opcode::INPUT);
        
        // Store to variable if right is identifier
        if (binop->right->kind == ASTNodeKind::IDENTIFIER) {
            auto id = static_cast<const Identifier*>(binop->right.get());
            auto sym = findSymbol(id->name);
            if (sym) {
                if (sym->type == Symbol::PARAMETER) {
                    // Parameters use BP-relative addressing
                    emit(Opcode::STORE_BP);
                    emitInt32(sym->offset);
                } else {
                    // Variables use absolute addressing
                    emit(Opcode::PUSH);
                    emitInt32(sym->offset);
                    emit(Opcode::STORE);
                }
            }
        } else if (binop->right->kind == ASTNodeKind::ARRAY_SUBSCRIPT) {
            // Store to array element: cin >> arr[i]
            auto sub = static_cast<const ArraySubscript*>(binop->right.get());
            if (sub->array->kind == ASTNodeKind::IDENTIFIER) {
                auto id = static_cast<const Identifier*>(sub->array.get());
                auto sym = findSymbol(id->name);
                
                if (sym) {
                    // Push base address
                    if (sym->type == Symbol::PARAMETER && sym->is_array) {
                        emit(Opcode::LOAD_BP);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_heap_allocated) {
                        emit(Opcode::LOAD);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_array) {
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    } else {
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    }
                    
                    // Push index and add to get element address
                    genExpression(sub->index.get());
                    emit(Opcode::ADD);
                    
                    // Stack: [input_value, addr]
                    emit(Opcode::STORE_INDIRECT);
                }
            }
        }
        
        // Push dummy value for result
        emit(Opcode::PUSH);
        emitInt32(0);
        return;
    }
    
    // Regular binary operations
    
    // --- Float arithmetic ---
    bool leftIsFloat = isFloatExpr(binop->left.get());
    bool rightIsFloat = isFloatExpr(binop->right.get());
    bool eitherFloat = leftIsFloat || rightIsFloat;
    
    if (eitherFloat && (binop->op == "+" || binop->op == "-" ||
                        binop->op == "*" || binop->op == "/")) {
        genExpression(binop->left.get());
        if (!leftIsFloat) emit(Opcode::INT_TO_FP);
        genExpression(binop->right.get());
        if (!rightIsFloat) emit(Opcode::INT_TO_FP);
        if (binop->op == "+") emit(Opcode::FADD);
        else if (binop->op == "-") emit(Opcode::FSUB);
        else if (binop->op == "*") emit(Opcode::FMUL);
        else emit(Opcode::FDIV);
        return;
    }
    
    // --- Float comparisons (result is int 0/1 on int stack) ---
    if (eitherFloat && (binop->op == "<"  || binop->op == ">" ||
                        binop->op == "<=" || binop->op == ">=" ||
                        binop->op == "==" || binop->op == "!=")) {
        genExpression(binop->left.get());
        if (!leftIsFloat) emit(Opcode::INT_TO_FP);
        genExpression(binop->right.get());
        if (!rightIsFloat) emit(Opcode::INT_TO_FP);
        
        std::string end_label  = makeLabel("fcmp_end");
        
        if (binop->op == "==" || binop->op == "!=") {
            emit(Opcode::FCMP);
            std::string unequal_label = makeLabel("fcmp_unequal");
            emitJump(Opcode::JL, unequal_label);
            emitJump(Opcode::JG, unequal_label);
            emit(Opcode::PUSH); emitInt32(binop->op == "==" ? 1 : 0);
            emitJump(Opcode::JMP, end_label);
            defineLabel(unequal_label);
            emit(Opcode::PUSH); emitInt32(binop->op == "==" ? 0 : 1);
            defineLabel(end_label);
        } else {
            // Use FCMP (sets cmp_flag) + conditional jump
            std::string true_label = makeLabel("fcmp_true");
            emit(Opcode::FCMP);
            Opcode jmpOp = (binop->op == "<")  ? Opcode::JL  :
                           (binop->op == ">")  ? Opcode::JG  :
                           (binop->op == "<=") ? Opcode::JLE : Opcode::JGE;
            emitJump(jmpOp, true_label);
            emit(Opcode::PUSH); emitInt32(0);
            emitJump(Opcode::JMP, end_label);
            defineLabel(true_label);
            emit(Opcode::PUSH); emitInt32(1);
            defineLabel(end_label);
        }
        return;
    }
    
    genExpression(binop->left.get());
    genExpression(binop->right.get());
    
    if (binop->op == "+") {
        emit(Opcode::ADD);
    } else if (binop->op == "-") {
        emit(Opcode::SUB);
    } else if (binop->op == "*") {
        emit(Opcode::MUL);
    } else if (binop->op == "/") {
        emit(Opcode::DIV);
    } else if (binop->op == "%") {
        emit(Opcode::MOD);
    } else if (binop->op == "<") {
        emit(Opcode::CMP);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emitJump(Opcode::JL, true_label);
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
    } else if (binop->op == ">") {
        emit(Opcode::CMP);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emitJump(Opcode::JG, true_label);
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
    } else if (binop->op == "<=") {
        emit(Opcode::CMP);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emitJump(Opcode::JLE, true_label);
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
    } else if (binop->op == ">=") {
        emit(Opcode::CMP);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emitJump(Opcode::JGE, true_label);
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
    } else if (binop->op == "==") {
        // For equality, we need cmp_flag == 0
        // Use SUB and check if result is 0
        emit(Opcode::SUB);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emit(Opcode::DUP);  // Duplicate result
        emitJump(Opcode::JZ, true_label);  // Jump if zero (equal)
        emit(Opcode::POP);  // Pop the duplicate
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::POP);  // Pop the duplicate
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
    } else if (binop->op == "!=") {
        // For inequality, result != 0
        emit(Opcode::SUB);
        std::string true_label = makeLabel("cmp_true");
        std::string end_label = makeLabel("cmp_end");
        emit(Opcode::DUP);
        emitJump(Opcode::JZ, true_label);  // Jump if zero (equal -> false for !=)
        emit(Opcode::POP);
        emit(Opcode::PUSH);
        emitInt32(1);  // Not equal -> true
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::POP);
        emit(Opcode::PUSH);
        emitInt32(0);  // Equal -> false
        defineLabel(end_label);
    } else {
        throw std::runtime_error("Unsupported binary operator '" + binop->op +
                                 "' at line " + std::to_string(binop->line));
    }
}

void CodeGenerator::genUnaryOp(const UnaryOp* unop) {
    if (unop->op == "new") {
        // new operator: allocate heap memory
        // Operand should be a type or size expression
        // For "new int[size]" we get the size, for "new int" we allocate 1
        if (unop->operand->kind == ASTNodeKind::ARRAY_SUBSCRIPT) {
            // new int[size] - array allocation
            auto sub = static_cast<const ArraySubscript*>(unop->operand.get());
            genExpression(sub->index.get());  // Push size
            emit(Opcode::ALLOC);
        } else {
            // new int - single allocation
            emit(Opcode::PUSH);
            emitInt32(1);  // Allocate 1 cell
            emit(Opcode::ALLOC);
        }
        return;
    }
    
    if (unop->op == "delete") {
        // delete operator: free heap memory
        genExpression(unop->operand.get());  // Push address
        emit(Opcode::FREE);
        // Push dummy value since it's an expression
        emit(Opcode::PUSH);
        emitInt32(0);
        return;
    }
    
    if (unop->op == "&") {
        // Address-of operator: return memory address of variable
        if (unop->operand->kind == ASTNodeKind::IDENTIFIER) {
            auto id = static_cast<const Identifier*>(unop->operand.get());
            auto sym = findSymbol(id->name);
            if (sym && sym->type == Symbol::VARIABLE) {
                // Push the address (offset) of the variable
                emit(Opcode::PUSH);
                emitInt32(sym->offset);
                return;
            }
        } else if (unop->operand->kind == ASTNodeKind::ARRAY_SUBSCRIPT) {
            // Address-of array element: &arr[index]
            auto sub = static_cast<const ArraySubscript*>(unop->operand.get());
            if (sub->array->kind == ASTNodeKind::IDENTIFIER) {
                auto id = static_cast<const Identifier*>(sub->array.get());
                auto sym = findSymbol(id->name);
                
                if (sym) {
                    // Push base address
                    if (sym->type == Symbol::PARAMETER && sym->is_array) {
                        emit(Opcode::LOAD_BP);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_heap_allocated) {
                        emit(Opcode::LOAD);
                        emitInt32(sym->offset);
                    } else if (sym->type == Symbol::VARIABLE && sym->is_array) {
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    } else {
                        emit(Opcode::PUSH);
                        emitInt32(sym->offset);
                    }
                    
                    // Push index and add to get element address
                    genExpression(sub->index.get());
                    emit(Opcode::ADD);
                    return;
                }
            }
        }
        throw std::runtime_error("Unsupported address-of expression at line " +
                                 std::to_string(unop->line));
    }
    
    if (unop->op == "*") {
        // Dereference operator: load value from address in operand
        genExpression(unop->operand.get()); // This should push an address
        // Now we have address on stack, load value from that address
        emit(Opcode::LOAD_INDIRECT);
        return;
    }

    if (unop->op == "!" ) {
        genCondition(unop->operand.get());
        std::string true_label = makeLabel("not_true");
        std::string end_label = makeLabel("not_end");
        emitJump(Opcode::JZ, true_label);
        emit(Opcode::PUSH);
        emitInt32(0);
        emitJump(Opcode::JMP, end_label);
        defineLabel(true_label);
        emit(Opcode::PUSH);
        emitInt32(1);
        defineLabel(end_label);
        return;
    }

    if (unop->op == "++" || unop->op == "--" ||
        unop->op == "++_post" || unop->op == "--_post") {
        if (unop->operand->kind != ASTNodeKind::IDENTIFIER) {
            throw std::runtime_error("Increment/decrement currently requires a variable");
        }
        auto id = static_cast<const Identifier*>(unop->operand.get());
        auto sym = findSymbol(id->name);
        if (!sym) throw std::runtime_error("Unknown identifier '" + id->name + "'");
        if (sym->is_float || sym->is_array) {
            throw std::runtime_error("Increment/decrement currently supports integer variables only");
        }
        const bool postfix = unop->op.find("_post") != std::string::npos;
        const bool increment = unop->op.rfind("++", 0) == 0;
        genIdentifier(id);
        if (postfix) emit(Opcode::DUP);
        emit(Opcode::PUSH);
        emitInt32(1);
        emit(increment ? Opcode::ADD : Opcode::SUB);
        if (!postfix) emit(Opcode::DUP);
        if (sym->type == Symbol::PARAMETER) {
            emit(Opcode::STORE_BP);
            emitInt32(sym->offset);
        } else {
            emit(Opcode::PUSH);
            emitInt32(sym->offset);
            emit(Opcode::STORE);
        }
        return;
    }
    
    // Other unary operators
    genExpression(unop->operand.get());
    
    if (unop->op == "-") {
        if (isFloatExpr(unop->operand.get())) {
            emit(Opcode::FNEG);
        } else {
            // Negate: push 0, swap, subtract
            emit(Opcode::PUSH);
            emitInt32(0);
            emit(Opcode::SWAP);
            emit(Opcode::SUB);
        }
    } else if (unop->op == "+") {
        // Unary plus does nothing
    } else if (unop->op == "~") {
        emit(Opcode::PUSH);
        emitInt32(0);
        emit(Opcode::SWAP);
        emit(Opcode::SUB);
        emit(Opcode::PUSH);
        emitInt32(1);
        emit(Opcode::SUB);
    } else {
        throw std::runtime_error("Unsupported unary operator '" + unop->op + "'");
    }
}

void CodeGenerator::genCall(const CallExpr* call) {
    // Get function name from callee
    if (call->callee->kind == ASTNodeKind::IDENTIFIER) {
        auto id = static_cast<const Identifier*>(call->callee.get());
        
        // Check if this is a constructor call (class/struct name)
        if (class_names.find(id->name) != class_names.end()) {
            throw std::runtime_error("Object construction is not supported by code generation");
        }
        
        // Special handling for print
        if (id->name == "print") {
            for (const auto& arg : call->args) {
                genExpression(arg.get());
                if (arg->kind == ASTNodeKind::LITERAL &&
                    static_cast<const Literal*>(arg.get())->litType == TokenType::STRING) {
                    emit(Opcode::PRINT_STR);
                } else if (isFloatExpr(arg.get())) {
                    emit(Opcode::FPRINT);
                } else {
                    emit(Opcode::PRINT);
                }
            }
            emit(Opcode::PUSH);
            emitInt32(0);
            return;
        }
        
        // Special handling for println (print with newline)
        if (id->name == "println") {
            for (const auto& arg : call->args) {
                genExpression(arg.get());
                if (arg->kind == ASTNodeKind::LITERAL) {
                    auto lit = static_cast<const Literal*>(arg.get());
                    if (lit->litType == TokenType::STRING) {
                        emit(Opcode::PRINT_STR);
                    } else if (isFloatExpr(arg.get())) {
                        emit(Opcode::FPRINT);
                    } else {
                        emit(Opcode::PRINT);
                    }
                } else if (isFloatExpr(arg.get())) {
                    emit(Opcode::FPRINT);
                } else {
                    emit(Opcode::PRINT);
                }
            }
            // Print newline
            int str_id = addString("\n");
            emit(Opcode::PUSH);
            emitInt32(str_id);
            emit(Opcode::PRINT_STR);
            emit(Opcode::PUSH);
            emitInt32(0);
            return;
        }
        
        // Regular function call. VM cdecl-style convention:
        // caller pushes arguments right-to-left and cleans them after RET.
        int arg_count = call->args.size();
        std::vector<bool> argument_types;
        argument_types.reserve(call->args.size());
        for (const auto& arg : call->args) argument_types.push_back(isFloatExpr(arg.get()));
        std::string mangled_name = mangleFunctionName(id->name, argument_types);
        auto sig_it = functions.find(mangled_name);
        const FunctionSignature* sig = (sig_it != functions.end()) ? &sig_it->second : nullptr;

        for (int i = arg_count - 1; i >= 0; --i) {
            const auto& arg = call->args[static_cast<size_t>(i)];
            bool expr_is_float = isFloatExpr(arg.get());
            bool param_is_float = sig && static_cast<size_t>(i) < sig->params_float.size()
                                  && sig->params_float[static_cast<size_t>(i)];

            genExpression(arg.get());
            if (param_is_float) {
                if (!expr_is_float) {
                    emit(Opcode::INT_TO_FP);
                }
                emit(Opcode::FP_TO_BITS);
            } else if (expr_is_float) {
                emit(Opcode::FP_TO_INT);
            }
        }

        Logger::debug() << "DBG genCall: calling '" << id->name << "' with " << arg_count
                        << " args -> mangled: '" << mangled_name << "'" << std::endl;
        emitJump(Opcode::CALL, mangled_name);

        if (arg_count > 0) {
            bool returns_float = sig && sig->returns_float;
            if (returns_float) {
                for (int i = 0; i < arg_count; i++) {
                    emit(Opcode::POP);
                }
            } else {
                for (int i = 0; i < arg_count; i++) {
                    emit(Opcode::SWAP);
                    emit(Opcode::POP);
                }
            }
        }
        return;
    }
    throw std::runtime_error("Only direct function calls are supported");
}

void CodeGenerator::genLiteral(const Literal* lit) {
    // Handle string literals
    if (lit->litType == TokenType::STRING) {
        int str_id = addString(lit->value);
        emit(Opcode::PUSH_STR);
        emitInt32(str_id);
        return;
    }
    
    // Check for float literal - emit to FPU stack
    if (lit->litType == TokenType::NUMBER && isFloatLiteralStr(lit->value)) {
        float fval = 0.0f;
        try {
            fval = std::stof(lit->value);
        } catch (...) {
            Logger::error() << "Warning: Could not parse float literal: " << lit->value << "\n";
        }
        emit(Opcode::FPUSH);
        emitFloat32(fval);
        return;
    }
    
    // Parse integer literal value
    int value = 0;
    
    // Check for character literal (single character, no quotes in stored value)
    if (lit->litType == TokenType::CHARACTER || 
        (lit->value.length() == 1 && !std::isdigit(lit->value[0]))) {
        // Single non-digit character - treat as character literal
        value = static_cast<int>(lit->value[0]);
    } else {
        try {
            value = std::stoi(lit->value);
        } catch (...) {
            // Try as float, convert to int
            try {
                value = static_cast<int>(std::stof(lit->value));
            } catch (...) {
                Logger::error() << "Warning: Could not parse literal: " << lit->value << "\n";
            }
        }
    }
    
    emit(Opcode::PUSH);
    emitInt32(value);
}

void CodeGenerator::genIdentifier(const Identifier* id) {
    // Handle special C++ identifiers that don't generate code
    if (id->name == "std" || id->name == "cout" || id->name == "cin" || 
        id->name == "endl" || id->name == "cerr") {
        // These are C++ I/O related - push 0 as placeholder
        emit(Opcode::PUSH);
        emitInt32(0);
        return;
    }
    
    auto sym = findSymbol(id->name);
    if (sym) {
        if (sym->type == Symbol::VARIABLE) {
            if (sym->is_float) {
                // Float variable: load from float_memory to FPU stack
                emit(Opcode::FLOAD);
                emitInt32(sym->offset);
            } else if (sym->is_heap_allocated) {
                // Heap-allocated arrays: load the heap pointer
                emit(Opcode::LOAD);
                emitInt32(sym->offset);
            } else if (sym->is_array) {
                // Stack arrays: push the stack address (pointer decay)
                emit(Opcode::PUSH);
                emitInt32(sym->offset);
            } else {
                // Regular variables: load the value
                emit(Opcode::LOAD);
                emitInt32(sym->offset);
            }
        } else if (sym->type == Symbol::PARAMETER) {
            if (sym->is_float) {
                emit(Opcode::LOAD_BP);
                emitInt32(sym->offset);
                emit(Opcode::BITS_TO_FP);
            } else if (sym->is_array) {
                // For array/pointer parameters: push the value (which is already an address)
                emit(Opcode::LOAD_BP);
                emitInt32(sym->offset);
            } else {
                // For regular parameters: load the value
                emit(Opcode::LOAD_BP);
                emitInt32(sym->offset);
            }
        } else if (sym->type == Symbol::FUNCTION) {
            // Function identifier used as value - push function address
            emit(Opcode::PUSH);
            emitInt32(sym->address);
        }
    } else {
        throw std::runtime_error("Unknown identifier '" + id->name + "' at line " +
                                 std::to_string(id->line));
    }
}

// Helper methods
void CodeGenerator::emit(Opcode op) {
    bytecode.push_back(static_cast<uint8_t>(op));
}

void CodeGenerator::emitByte(uint8_t byte) {
    bytecode.push_back(byte);
}

void CodeGenerator::emitInt32(int32_t value) {
    // Little-endian
    bytecode.push_back(value & 0xFF);
    bytecode.push_back((value >> 8) & 0xFF);
    bytecode.push_back((value >> 16) & 0xFF);
    bytecode.push_back((value >> 24) & 0xFF);
}

void CodeGenerator::emitInt32At(size_t pos, int32_t value) {
    bytecode[pos] = value & 0xFF;
    bytecode[pos+1] = (value >> 8) & 0xFF;
    bytecode[pos+2] = (value >> 16) & 0xFF;
    bytecode[pos+3] = (value >> 24) & 0xFF;
}

void CodeGenerator::emitFloat32(float value) {
    uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(float));
    for (int i = 0; i < 4; i++) bytecode.push_back(bytes[i]);
}

// Returns true if the literal string represents a floating-point number
bool CodeGenerator::isFloatLiteralStr(const std::string& s) {
    if (s.empty()) return false;
    // Hex integers are not float
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return false;
    for (char c : s) {
        if (c == '.' || c == 'e' || c == 'E') return true;
    }
    return false;
}

// Returns true if the type token list represents float or double
bool CodeGenerator::isFloatType(const std::vector<std::string>& typeTokens) {
    for (const auto& t : typeTokens) {
        if (t == "float" || t == "double") return true;
    }
    return false;
}

// Returns true if the expression produces a float value (leaving it on the FPU stack)
bool CodeGenerator::isFloatExpr(const ASTNode* node) {
    if (!node) return false;
    switch (node->kind) {
        case ASTNodeKind::LITERAL: {
            auto lit = static_cast<const Literal*>(node);
            if (lit->litType == TokenType::STRING || lit->litType == TokenType::CHARACTER)
                return false;
            return isFloatLiteralStr(lit->value);
        }
        case ASTNodeKind::IDENTIFIER: {
            auto id = static_cast<const Identifier*>(node);
            auto sym = findSymbol(id->name);
            return sym && sym->is_float;
        }
        case ASTNodeKind::BINARY_OP: {
            auto bin = static_cast<const BinaryOp*>(node);
            // Assignment result type follows the left-hand side
            if (bin->op == "=") {
                if (bin->left->kind == ASTNodeKind::IDENTIFIER) {
                    auto id = static_cast<const Identifier*>(bin->left.get());
                    auto sym = findSymbol(id->name);
                    return sym && sym->is_float;
                }
                return false;
            }
            if (bin->op == "<" || bin->op == ">" || bin->op == "<=" ||
                bin->op == ">=" || bin->op == "==" || bin->op == "!=" ||
                bin->op == "&&" || bin->op == "||" || bin->op == "<<" ||
                bin->op == ">>") {
                return false;
            }
            return isFloatExpr(bin->left.get()) || isFloatExpr(bin->right.get());
        }
        case ASTNodeKind::CALL: {
            auto call = static_cast<const CallExpr*>(node);
            if (call->callee->kind != ASTNodeKind::IDENTIFIER) return false;
            auto id = static_cast<const Identifier*>(call->callee.get());
            std::vector<bool> argument_types;
            argument_types.reserve(call->args.size());
            for (const auto& arg : call->args) argument_types.push_back(isFloatExpr(arg.get()));
            std::string mangled_name = mangleFunctionName(id->name, argument_types);
            auto it = functions.find(mangled_name);
            return it != functions.end() && it->second.returns_float;
        }
        case ASTNodeKind::UNARY_OP: {
            auto un = static_cast<const UnaryOp*>(node);
            if (un->op == "!" || un->op == "~" || un->op == "&" ||
                un->op == "*" || un->op == "new" || un->op == "delete" ||
                un->op.find("++") != std::string::npos ||
                un->op.find("--") != std::string::npos) {
                return false;
            }
            return isFloatExpr(un->operand.get());
        }
        case ASTNodeKind::CONDITIONAL: {
            auto conditional = static_cast<const ConditionalExpr*>(node);
            return isFloatExpr(conditional->thenExpr.get()) ||
                   isFloatExpr(conditional->elseExpr.get());
        }
        default:
            return false;
    }
}

// Label management
std::string CodeGenerator::makeLabel(const std::string& prefix) {
    return prefix + "_" + std::to_string(label_counter++);
}

void CodeGenerator::defineLabel(const std::string& label) {
    if (labels[label].defined) {
        throw std::runtime_error("Duplicate function or label definition: " + label);
    }
    labels[label].address = currentAddress();
    labels[label].defined = true;
}

void CodeGenerator::emitJump(Opcode op, const std::string& label) {
    emit(op);
    labels[label].fixup_positions.push_back(currentAddress());
    emitInt32(0); // Placeholder
}

void CodeGenerator::fixupLabels() {
    for (auto& [name, label] : labels) {
        if (!label.defined) {
            throw std::runtime_error("Undefined function or label: " + name);
        }
        
        for (size_t pos : label.fixup_positions) {
            emitInt32At(pos, label.address);
        }
    }
}

// String table
int CodeGenerator::addString(const std::string& str) {
    // Check if string already exists
    for (size_t i = 0; i < string_table.size(); i++) {
        if (string_table[i] == str) {
            Logger::debug() << "DBG addString: existing id=" << i << " str='" << str << "'\n";
            return i;
        }
    }
    // Add new string
    string_table.push_back(str);
    int id = string_table.size() - 1;
    Logger::debug() << "DBG addString: new id=" << id << " str='" << str << "'\n";
    return id;
}

// Name mangling for function overloading
std::string CodeGenerator::mangleFunctionName(const std::string& name, int param_count) {
    // Simple name mangling: name_Pcount
    // e.g., foo_P2 for foo with 2 parameters
    if (param_count == 0) {
        return name;  // No mangling for parameterless functions
    }
    return name + "_P" + std::to_string(param_count);
}

std::string CodeGenerator::mangleFunctionName(const std::string& name, 
    const std::vector<std::pair<std::vector<std::string>, std::string>>& params) {
    std::vector<bool> float_params;
    float_params.reserve(params.size());
    for (const auto& param : params) {
        float_params.push_back(isFloatType(param.first));
    }
    return mangleFunctionName(name, float_params);
}

std::string CodeGenerator::mangleFunctionName(const std::string& name,
                                               const std::vector<bool>& params_float) {
    if (params_float.empty()) return name;
    std::string mangled = name + "_P" + std::to_string(params_float.size());
    for (bool is_float : params_float) mangled += is_float ? "_f" : "_i";
    return mangled;
}

// Symbol table
void CodeGenerator::addVariable(const std::string& name, int offset, bool is_array, bool is_heap_allocated, bool is_float) {
    Symbol sym;
    sym.type = Symbol::VARIABLE;
    sym.offset = offset;
    sym.address = 0;
    sym.param_count = 0;
    sym.is_array = is_array;
    sym.is_heap_allocated = is_heap_allocated;
    sym.is_float = is_float;
    rememberSymbolBeforeChange(name);
    symbols[name] = sym;
    
    Logger::debug() << "DBG addVariable: '" << name << "' offset=" << offset
                    << " is_array=" << is_array << std::endl;
}

void CodeGenerator::addParameter(const std::string& name, int offset) {
    Symbol sym;
    sym.type = Symbol::PARAMETER;
    sym.offset = offset;
    sym.address = 0;
    sym.param_count = 0;
    sym.is_array = false;  // Will be updated for pointer/array params
    sym.is_heap_allocated = false;
    sym.is_float = false;
    rememberSymbolBeforeChange(name);
    symbols[name] = sym;
}

void CodeGenerator::addFunction(const std::string& name, int address, int param_count) {
    Symbol sym;
    sym.type = Symbol::FUNCTION;
    sym.offset = 0;
    sym.address = address;
    sym.param_count = param_count;
    sym.is_array = false;
    sym.is_heap_allocated = false;
    sym.is_float = false;
    rememberSymbolBeforeChange(name);
    symbols[name] = sym;
}

Symbol* CodeGenerator::findSymbol(const std::string& name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

bool CodeGenerator::saveToFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        Logger::error() << "Error: Could not open file: " << filename << "\n";
        return false;
    }
    
    // Write string table size
    uint32_t str_count = string_table.size();
    Logger::debug() << "DBG saveToFile: str_count=" << str_count << "\n";
    file.write(reinterpret_cast<const char*>(&str_count), sizeof(str_count));
    
    // Write each string (length + data)
    for (const auto& str : string_table) {
        uint32_t len = str.length();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.data(), len);
    }
    
    // Write bytecode size
    uint32_t code_size = bytecode.size();
    file.write(reinterpret_cast<const char*>(&code_size), sizeof(code_size));
    
    // Write bytecode
    file.write(reinterpret_cast<const char*>(bytecode.data()), bytecode.size());
    return true;
}

void CodeGenerator::dumpBytecode() const {
    Logger::out() << "\n=== Function Labels (Name Mangling) ===\n";
    for (const auto& label : labels) {
        if (label.second.defined) {
            Logger::out() << "  " << label.first << " @ address " << label.second.address << std::endl;
        }
    }
    
    Logger::out() << "\n=== Generated Bytecode ===\n";
    Logger::out() << "Size: " << bytecode.size() << " bytes\n\n";
    
    for (size_t i = 0; i < bytecode.size(); ) {
        Logger::out() << std::setw(4) << std::setfill('0') << i << ": ";
        Logger::out() << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytecode[i]);
        
        Opcode op = static_cast<Opcode>(bytecode[i]);
        i++;
        
        // Show operands for instructions that have them
        if (op == Opcode::PUSH || op == Opcode::JMP || op == Opcode::JZ || 
            op == Opcode::JNZ || op == Opcode::JL || op == Opcode::JG ||
            op == Opcode::JLE || op == Opcode::JGE || op == Opcode::CALL) {
            if (i + 4 <= bytecode.size()) {
                int32_t value = bytecode[i] | (bytecode[i+1] << 8) | 
                               (bytecode[i+2] << 16) | (bytecode[i+3] << 24);
                Logger::out() << " " << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+1]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+2]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+3]);
                Logger::out() << " (" << std::dec << value << ")";
                i += 4;
            }
        }
        
        Logger::out() << std::dec << "\n";
    }
    Logger::out() << "\n";
}

void CodeGenerator::enterScope() {
    scope_changes.emplace_back();
}

void CodeGenerator::exitScope() {
    if (scope_changes.empty()) {
        throw std::runtime_error("Internal code-generation scope underflow");
    }
    for (const auto& [name, previous] : scope_changes.back()) {
        if (previous) symbols[name] = *previous;
        else symbols.erase(name);
    }
    scope_changes.pop_back();
}

void CodeGenerator::rememberSymbolBeforeChange(const std::string& name) {
    if (scope_changes.empty()) return;
    auto& changes = scope_changes.back();
    if (changes.find(name) != changes.end()) return;
    auto existing = symbols.find(name);
    if (existing == symbols.end()) changes[name] = std::nullopt;
    else changes[name] = existing->second;
}


void CodeGenerator::genArraySubscript(const ArraySubscript* sub) {
    // Calculate address: base + index
    // Leaves address on stack
    
    if (sub->array->kind == ASTNodeKind::IDENTIFIER) {
        auto id = static_cast<const Identifier*>(sub->array.get());
        auto sym = findSymbol(id->name);
        
        if (sym) {
            // Push base address
            if (sym->type == Symbol::PARAMETER && sym->is_array) {
                // For array parameters: load the pointer value stored in parameter
                emit(Opcode::LOAD_BP);
                emitInt32(sym->offset);
            } else if (sym->type == Symbol::VARIABLE && sym->is_heap_allocated) {
                // For heap-allocated arrays: load the heap address from variable
                emit(Opcode::LOAD);
                emitInt32(sym->offset);
            } else if (sym->type == Symbol::VARIABLE && sym->is_array) {
                // For stack arrays: use the stack address
                emit(Opcode::PUSH);
                emitInt32(sym->offset);
            } else {
                // For regular variables/parameters
                emit(Opcode::PUSH);
                emitInt32(sym->offset);
            }
            
            // Evaluate and push index
            genExpression(sub->index.get());
            
            // Add them: base + index -> element address on stack
            emit(Opcode::ADD);
            
            // Load value using indirect addressing
            emit(Opcode::LOAD_INDIRECT);
            return;
        }
        throw std::runtime_error("Unknown array '" + id->name + "'");
    }
    throw std::runtime_error("Unsupported array subscript expression");
}
