# GOC - Experimental C++ Compiler Frontend & Virtual Machine (Beta)

GOC (GreyOne Compiler) is an early **beta-version** compiler environment written entirely from scratch — including lexer, parser, AST builder, bytecode generator, and a custom stack-based virtual machine. It is already capable of compiling and executing a subset of C++ code, demonstrating a fully working end-to-end compilation pipeline.

This project represents my **first deep dive into compiler engineering**, and **as a flex I decided to show some compiler theory stuff — I basically turned C++ into Java )))**, just to demonstrate how flexible the pipeline already is., heavily inspired by the concepts, theory, and algorithms presented in the legendary "Dragon Book" (*Compilers: Principles, Techniques, and Tools*). I used its ideas as a foundation, but implemented every part myself — from lexical analysis to VM execution.

---

## 🚀 Features (Current Beta)

### ✔️ Lexical Analysis

* Tokenizes C++-like input
* Supports identifiers, keywords, operators, numbers, braces, and preprocessors
* Outputs detailed token statistics

### ✔️ Syntax Analysis

* Recursive-descent parser for simplified C++ grammar
* Builds an **Abstract Syntax Tree (AST)**
* Supports:

  * variable declarations
  * arithmetic expressions
  * comparison operators
  * `while` loops
  * simple statements
  * `std::cout` output chains

### ✔️ Code Generation

* Walks the AST and emits bytecode for a custom stack-based virtual machine
* Machine operations include:

  * Load/store
  * Arithmetic ops
  * Comparisons
  * Conditional and unconditional jumps
  * Print operations

### ✔️ Virtual Machine (VM)

* Executes GOC bytecode
* Prints values during runtime
* Implements a clean instruction set and stack behavior
* Supports simple control flow, counters, and output

---

## 📌 Example End-to-End Compilation

Input file:

```cpp
#include <iostream>

int main() {
    int i = 0;
    while(i < 10) {
        i = i + 1;
        std::cout << i;
    }
}
```

GOC compiles this into bytecode and the VM executes it, producing:

```
1
2
3
4
5
6
7
8
9
10
VM halted
```

This demonstrates the full pipeline working correctly.

---

## 🔥 Technical Architecture

### Note on Implementation History

Originally, the Virtual Machine backend was written in **NASM x86_64 assembly**, but was later fully **rewritten in C for comfort, portability, and easier debugging**. The current VM is cleaner and more maintainable while preserving the original low-level design philosophy.

---

### 1. Lexer (`Lexer`)

* Processes the input file character-by-character
* Generates tokens using deterministic scanning rules
* Inspired by Dragon Book lexical design, but tailored manually

### 2. Parser (`Parser`)

* Handwritten recursive-descent parser
* Grammar follows LL(1)-friendly patterns
* Produces typed AST nodes:

  * `BinaryExpression`
  * `VariableDeclaration`
  * `WhileStatement`
  * `PrintStatement`
  * etc.

### 3. AST Representation

* Every language construct is represented as a node
* Nodes are used directly by the code generator

### 4. Bytecode Generator (`Codegen`)

* Emits VM-friendly instructions
* Manages temporary stack usage
* Produces relocatable jump offsets

### 5. Virtual Machine (`VM`)

* Simple but powerful stack machine
* Fetch-decode-execute loop
* Instruction set includes:

  * `LOAD_CONST`
  * `LOAD_LOCAL`
  * `STORE_LOCAL`
  * `ADD`, `SUB`, `MUL`, `DIV`
  * `LT`, `GT`, `EQ`
  * `JMP`, `JMP_IF_FALSE`
  * `PRINT`

---

## 🧪 Beta Status

This project **skips full semantic analysis and performs no optimization passes**. As a result, the workflow may be unstable in edge cases — the compiler assumes valid code structure and may generate incorrect bytecode for ambiguous or invalid constructs.

Current limitations due to missing semantic analysis:

* No type-checking enforcement
* Undeclared variables may not be caught correctly
* No scope resolution rules
* No validation of operator types
* Potential undefined behavior during code generation

These parts are intentionally postponed to keep focus on architectural clarity and iterative learning.

This project still contains **placeholders for more complex C++ features**, such as:

* namespaces
* lambdas
* containers (std::vector, std::map, etc.)
* advanced expression handling
* fully compliant operator chains
* complete semantic analysis

These placeholders exist so the architecture can evolve naturally without breaking the current functional beta.

This is a **beta** version, meaning:

* language coverage is incomplete
* grammar is simplified
* error recovery is basic
* bytecode is not optimized
* VM uses minimal instruction set

**However:** the entire system is stable enough to compile real toy programs and execute them inside a controlled environment.

---

## 🎯 Design Philosophy

* Build everything **from zero**, no external parsing libraries
* Understand each component deeply
* Keep the architecture modular and educational
* Make debugging easy via verbose logs
* Allow future extension toward a real optimizing compiler

---

## 📚 Inspiration

This project is strongly influenced by:

### **The Dragon Book**

*Compilers: Principles, Techniques, and Tools* by Aho, Lam, Sethi, and Ullman.

Concepts borrowed:

* token definitions
* recursive-descent parsing strategies
* AST transformations
* stack machine design principles
* code generation patterns

But the implementation is entirely my own.

---

## 🛠️ Planned Features / Roadmap

* Support for `if` / `else`
* Functions and call stack
* Return values
* Local variable scopes
* String literals
* Real `std::cout << a << b` operator chaining
* Optimizer pass
* Disassembler for bytecode
* Register-based VM option
* Better error reporting

---

## 🤝 Contribution & Notes

This is a personal educational project. Contributions are welcome, but please understand:

* The architecture is intentionally low-level for learning
* Large rewrites may not align with the project’s educational purpose

---

## 📜 License

Open-source, free for learning, modification, and experimentation.

---

## 💬 Final Words

GOC is my **first real compiler**, built out of passion and curiosity. It's a massive learning journey inspired by the Dragon Book and powered by a desire to understand compilers from the inside out.

Even in beta, it already forms a fully functioning compilation toolchain.

More features, documentation, and refactoring coming soon.

**- Sergey Mkhoyan 3rd-semester group-402 (GreyOne)**
