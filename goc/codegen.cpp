#include "codegen.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
CodeGenerator::CodeGenerator() 
    : current_offset(0), next_memory_addr(0), label_counter(0) {
}

std::vector<uint8_t> CodeGenerator::generate(const Program& program) {
    bytecode.clear();
    symbols.clear();
    current_offset = 0;
    next_memory_addr = 0;
    label_counter = 0;
    
    genProgram(program);
    fixupLabels();
    
    return bytecode;
}

void CodeGenerator::genProgram(const Program& prog) {
    // Emit entry point that calls main and halts
    emitJump(Opcode::CALL, "main");
    emit(Opcode::HALT);
    
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

    // Generate code for all top-level declarations and class member functions
    for (const auto& node : prog.top) {
        if (!node) continue;  // Safety check
        if (node->kind == ASTNodeKind::CLASS_DECL) {
            auto cls = static_cast<const ClassDecl*>(node.get());
            if (!cls) continue;
            // Emit member functions as separate functions
            for (const auto& m : cls->members) {
                if (m && m->kind == ASTNodeKind::FUNC_DECL) {
                    auto f = static_cast<const FunctionDecl*>(m.get());
                    if (!f) continue;
                    // Create qualified label for method
                    std::string qual = cls->className + "::" + f->funcName;
                    // Emit function under qualified name by calling genFunctionDecl with override
                    genFunctionDecl(f, qual);
                }
            }
        } else {
            genStatement(node.get());
        }
    }
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
            std::cerr << "Warning: Unhandled statement type " 
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
    
    // Allocate memory address for this variable
    int addr = next_memory_addr++;
    addVariable(decl->varName, addr, is_array, is_heap_array, is_float_var);
    
    // If there's an initializer, evaluate it and store
    if (decl->init) {
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
    // Use simple name mangling for overloaded functions (param count only)
    std::string mangled_name = mangleFunctionName(func->funcName, func->params.size());
    genFunctionDecl(func, mangled_name);
}

void CodeGenerator::genFunctionDecl(const FunctionDecl* func, const std::string& nameOverride) {
    // Define function label
    // DEBUG: // std::cerr << "DBG genFunctionDecl: defining label '" << nameOverride 
// DEBUG_CONT:               << "' at address " << currentAddress() << std::endl;
    defineLabel(nameOverride);
    
    // Function prologue
    emit(Opcode::PUSH_BP);
    
    // Parameters are on stack below saved BP
    // After PUSH_BP, stack layout:
    // [... caller stuff, arg1, arg2, saved_BP] <- BP points here
    // So: BP-1 = saved_BP, BP-2 = arg2, BP-3 = arg1
    // Parameters need negative offsets relative to BP
    int param_count = func->params.size();
    for (int i = 0; i < param_count; i++) {
        // First param is at BP-(param_count+1), last param at BP-2
        int offset = -(param_count - i + 1);
        
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
        sym.is_array = is_pointer;  // Pointers and arrays treated same
        symbols[func->params[i].second] = sym;
        
        // DEBUG: // std::cerr << "DBG addParam: '" << func->params[i].second 
// DEBUG_CONT:                   << "' offset=" << offset << " is_array=" << is_pointer << std::endl;
    }
    
    // Generate function body
    if (func->body) {
        genStatement(func->body.get());
    }
    
    // Function epilogue (if no explicit return)
    emit(Opcode::POP_BP);
    emit(Opcode::RET);
}

void CodeGenerator::genBlock(const BlockStmt* block) {
    for (const auto& stmt : block->statements) {
        genStatement(stmt.get());
    }
}

void CodeGenerator::genIf(const IfStmt* ifstmt) {
    std::string else_label = makeLabel("else");
    std::string end_label = makeLabel("endif");
    
    // Evaluate condition
    genExpression(ifstmt->cond.get());
    
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
    genExpression(whilestmt->cond.get());
    
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
        genExpression(forstmt->cond.get());
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
        case ASTNodeKind::MEMBER_ACCESS:
            // For now, handle member access specially
            // std::cout and other I/O operations
            // Just push 0 as placeholder (no real I/O in VM yet)
            emit(Opcode::PUSH);
            emitInt32(0);
            break;
        default:
            std::cerr << "Warning: Unhandled expression type " 
                      << static_cast<int>(node->kind) << " in codegen\n";
            emit(Opcode::PUSH);
            emitInt32(0); // Push dummy value
            break;
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
                }
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
                    emit(Opcode::FDUP);              // keep copy for expression result
                    emit(Opcode::FSTORE);
                    emitInt32(sym->offset);
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
            }
        }
        return;
    }
    
    // Handle << operator (stream output operator)
    if (binop->op == "<<") {
        // Special-case: chained cout << a << b << endl
        // If left is identifier std::cout or a chain starting from it, handle print
        const ASTNode* cur = binop;
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
        
        std::string true_label = makeLabel("fcmp_true");
        std::string end_label  = makeLabel("fcmp_end");
        
        if (binop->op == "==" || binop->op == "!=") {
            // Use FSUB + FP_TO_INT + JZ
            emit(Opcode::FSUB);
            emit(Opcode::FP_TO_INT);
            emit(Opcode::DUP);
            emitJump(Opcode::JZ, true_label);
            emit(Opcode::POP);
            emit(Opcode::PUSH); emitInt32(binop->op == "==" ? 0 : 1);
            emitJump(Opcode::JMP, end_label);
            defineLabel(true_label);
            emit(Opcode::POP);
            emit(Opcode::PUSH); emitInt32(binop->op == "==" ? 1 : 0);
            defineLabel(end_label);
        } else {
            // Use FCMP (sets cmp_flag) + conditional jump
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
        // Unknown operator - just evaluate operands and push 0
        emit(Opcode::POP);
        emit(Opcode::POP);
        emit(Opcode::PUSH);
        emitInt32(0);
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
        // Default: push 0 for unsupported address-of
        emit(Opcode::PUSH);
        emitInt32(0);
        return;
    }
    
    if (unop->op == "*") {
        // Dereference operator: load value from address in operand
        genExpression(unop->operand.get()); // This should push an address
        // Now we have address on stack, load value from that address
        emit(Opcode::LOAD_INDIRECT);
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
    }
}

void CodeGenerator::genCall(const CallExpr* call) {
    // Get function name from callee
    if (call->callee->kind == ASTNodeKind::IDENTIFIER) {
        auto id = static_cast<const Identifier*>(call->callee.get());
        
        // Check if this is a constructor call (class/struct name)
        if (class_names.find(id->name) != class_names.end()) {
            // Skip constructor calls - just push dummy value for now
            // In a real implementation, this would allocate an object
            emit(Opcode::PUSH);
            emitInt32(0);
            return;
        }
        
        // Special handling for print
        if (id->name == "print") {
            for (const auto& arg : call->args) {
                genExpression(arg.get());
                if (isFloatExpr(arg.get())) emit(Opcode::FPRINT);
                else emit(Opcode::PRINT);
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
        
        // Regular function call - push arguments first
        int arg_count = call->args.size();
        for (const auto& arg : call->args) {
            genExpression(arg.get());
        }
        
        // Use name mangling for function calls
        std::string mangled_name = mangleFunctionName(id->name, arg_count);
        // DEBUG: // std::cerr << "DBG genCall: calling '" << id->name << "' with " << arg_count 
// DEBUG_CONT:                   << " args -> mangled: '" << mangled_name << "'" << std::endl;
        emitJump(Opcode::CALL, mangled_name);
        
        // Clean up arguments from stack after return
        // Stack layout after RET: [arg1, arg2, ..., argN, retval]
        // We want: [retval]
        if (arg_count > 0) {
            // Stack after CALL: [arg1, ..., argN, retval]
            // Remove arguments while preserving retval using SWAP+POP per argument.
            for (int i = 0; i < arg_count; i++) {
                emit(Opcode::SWAP);
                emit(Opcode::POP);
            }
        }
    }
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
            std::cerr << "Warning: Could not parse float literal: " << lit->value << "\n";
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
                std::cerr << "Warning: Could not parse literal: " << lit->value << "\n";
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
            if (sym->is_array) {
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
        // Unknown identifier - might be external or C++ stdlib
        // For now, just push 0 as placeholder
        emit(Opcode::PUSH);
        emitInt32(0);
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
            return isFloatExpr(bin->left.get()) || isFloatExpr(bin->right.get());
        }
        case ASTNodeKind::UNARY_OP: {
            auto un = static_cast<const UnaryOp*>(node);
            return isFloatExpr(un->operand.get());
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
            std::cerr << "Error: Undefined label: " << name << "\n";
            continue;
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
            // DEBUG: // std::cerr << "DBG addString: existing id=" << i << " str='" << str << "'\n";
            return i;
        }
    }
    // Add new string
    string_table.push_back(str);
    int id = string_table.size() - 1;
    // DEBUG: // std::cerr << "DBG addString: new id=" << id << " str='" << str << "'\n";
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
    // More sophisticated mangling based on parameter types
    // Format: name_P<count>_<type1>_<type2>...
    if (params.empty()) {
        return name;
    }
    
    std::string mangled = name + "_P" + std::to_string(params.size());
    for (const auto& param : params) {
        if (!param.first.empty()) {
            // Use first token of type (simplified)
            std::string type = param.first[0];
            // Shorten common types
            if (type == "int") type = "i";
            else if (type == "float") type = "f";
            else if (type == "double") type = "d";
            else if (type == "char") type = "c";
            else if (type == "bool") type = "b";
            else if (type == "void") type = "v";
            else if (type == "std") type = "s";
            // For pointers/references
            if (param.first.size() > 1) {
                if (param.first.back() == "*") type += "p";
                if (param.first.back() == "&") type += "r";
            }
            mangled += "_" + type;
        }
    }
    return mangled;
}

// Symbol table
void CodeGenerator::addVariable(const std::string& name, int offset, bool is_array, bool is_heap_allocated, bool is_float) {
    Symbol sym;
    sym.type = Symbol::VARIABLE;
    sym.offset = offset;
    sym.is_array = is_array;
    sym.is_heap_allocated = is_heap_allocated;
    sym.is_float = is_float;
    symbols[name] = sym;
    
    // DEBUG: // std::cerr << "DBG addVariable: '" << name << "' offset=" << offset 
// DEBUG_CONT:               << " is_array=" << is_array << std::endl;
}

void CodeGenerator::addParameter(const std::string& name, int offset) {
    Symbol sym;
    sym.type = Symbol::PARAMETER;
    sym.offset = offset;
    sym.is_array = false;  // Will be updated for pointer/array params
    sym.is_heap_allocated = false;
    sym.is_float = false;
    symbols[name] = sym;
}

void CodeGenerator::addFunction(const std::string& name, int address, int param_count) {
    Symbol sym;
    sym.type = Symbol::FUNCTION;
    sym.address = address;
    sym.param_count = param_count;
    sym.is_array = false;
    sym.is_heap_allocated = false;
    sym.is_float = false;
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
        std::cerr << "Error: Could not open file: " << filename << "\n";
        return false;
    }
    
    // Write string table size
    uint32_t str_count = string_table.size();
    // DEBUG: // std::cerr << "DBG saveToFile: str_count=" << str_count << "\n";
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
    std::cout << "\n=== Function Labels (Name Mangling) ===\n";
    for (const auto& label : labels) {
        if (label.second.defined) {
            std::cout << "  " << label.first << " @ address " << label.second.address << std::endl;
        }
    }
    
    std::cout << "\n=== Generated Bytecode ===\n";
    std::cout << "Size: " << bytecode.size() << " bytes\n\n";
    
    for (size_t i = 0; i < bytecode.size(); ) {
        std::cout << std::setw(4) << std::setfill('0') << i << ": ";
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
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
                std::cout << " " << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+1]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+2]) << " "
                         << std::hex << std::setw(2) << std::setfill('0')
                         << static_cast<int>(bytecode[i+3]);
                std::cout << " (" << std::dec << value << ")";
                i += 4;
            }
        }
        
        std::cout << std::dec << "\n";
    }
    std::cout << "\n";
}

void CodeGenerator::enterScope() {
    // For future: implement scoping
}

void CodeGenerator::exitScope() {
    // For future: implement scoping
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
        }
    }
}
