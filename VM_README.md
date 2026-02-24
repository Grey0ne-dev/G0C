# GOC Compiler & Virtual Machine

Cross-platform C++ compiler frontend with stack-based bytecode VM.

## Components

### 1. Compiler Frontend (`goc`)
- **Lexer** (`lexer.cpp/.h`): Full C++ tokenization
- **Parser** (`parser.cpp/.h`): AST generation for C++ subset
- **CodeGen** (`codegen.cpp/.h`): Bytecode generation with name mangling

### 2. Virtual Machine (`vm`)
- **VM** (`vm.cpp/.h`): Stack-based bytecode interpreter
- **Cross-platform I/O**: Works on Windows (_WIN32) and Unix/Linux
- **Features**: Functions, variables, arithmetic, I/O, basic OOP support

## Building

```bash
mkdir build && cd build
cmake ..
make
```

This produces two executables:
- `goc` - the compiler
- `vm` - the virtual machine

## Usage

###  Compile
```bash
./goc source.cpp -o output.bin
```

### Run
```bash
./vm output.bin
```

### Debug
```bash
./goc source.cpp --dump-ast --dump-bytecode
./vm output.bin --debug --disassemble
```

## Name Mangling

Function overloading supported via name mangling:
- Format: `functionName_P<count>_<type1>_<type2>...`
- Example: `foo(int, float)` → `foo_P2_i_f`
- Member functions: `ClassName::method` → `ClassName::method`

## Opcodes

- **Stack**: PUSH, POP, DUP, SWAP
- **Arithmetic**: ADD, SUB, MUL, DIV, MOD
- **Control**: JMP, JZ, JNZ, JL, JG, JLE, JGE, CMP
- **Functions**: CALL, RET, PUSH_BP, POP_BP
- **Memory**: LOAD, STORE, LOAD_BP, STORE_BP, LOAD_INDIRECT, STORE_INDIRECT, ALLOC, FREE
- **I/O**: PRINT, INPUT, PRINT_STR, INPUT_STR, PUSH_STR
- **FPU/Float**: FPUSH, FPOP, FADD, FSUB, FMUL, FDIV, FLOAD, FSTORE, FPRINT, FCMP, FNEG, FDUP, INT_TO_FP, FP_TO_INT
- **Control**: HALT

## Frontend Issues Fixed

1. **Label fixup**: Functions now properly define labels before code generation
2. **genIdentifier bug**: LOAD instruction now correctly uses bytecode operand instead of stack-based addressing

## Platform Support

- Linux/Unix: Native support
- Windows: Uses `_WIN32` preprocessor directives
- I/O operations abstracted for cross-platform compatibility

## Data Structures Used

1. **std::vector**: Bytecode storage, stack, memory, string table
2. **std::unordered_map**: Symbol table, label fixups, objects
3. **std::stack** (via vector): Call frames for function calls

## Example

```cpp
int main() {
    int x = 10;
    int y = 20;
    int result = x + y;
    print(result);
    return 0;
}
```

Output: `30`
