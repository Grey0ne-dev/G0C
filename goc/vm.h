#ifndef VM_H
#define VM_H

#include <vector>
#include <stack>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>
#include <iostream>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
#endif

// Opcodes (matching codegen.h)
enum class VMOpcode : uint8_t {
    PUSH        = 0x01,
    POP         = 0x02,
    ADD         = 0x03,
    SUB         = 0x04,
    MUL         = 0x05,
    DIV         = 0x06,
    MOD         = 0x07,
    DUP         = 0x08,
    SWAP        = 0x09,
    PRINT       = 0x0A,
    PRINT_STR   = 0x0B,
    INPUT_STR   = 0x0C,
    INPUT       = 0x0D,
    JMP         = 0x10,
    JZ          = 0x11,
    JNZ         = 0x12,
    JL          = 0x13,
    JG          = 0x14,
    JLE         = 0x15,
    JGE         = 0x16,
    CMP         = 0x17,
    CALL        = 0x18,
    RET         = 0x19,
    LOAD        = 0x20,
    STORE       = 0x21,
    LOAD_BP     = 0x22,
    STORE_BP    = 0x23,
    PUSH_BP     = 0x24,
    POP_BP      = 0x25,
    PUSH_STR    = 0x26,
    LOAD_INDIRECT  = 0x27,  // Pop address, load mem[addr], push value
    STORE_INDIRECT = 0x28,  // Pop addr, pop value, store mem[addr] = value
    ALLOC       = 0x29,     // Pop size, allocate heap memory, push address
    FREE        = 0x2A,     // Pop address, free heap memory

    // FPU (x87-style circular register stack, 8 slots)
    FPUSH       = 0x30,
    FPOP        = 0x31,
    FADD        = 0x32,
    FSUB        = 0x33,
    FMUL        = 0x34,
    FDIV        = 0x35,
    FLOAD       = 0x36,
    FSTORE      = 0x37,
    FPRINT      = 0x38,
    FCMP        = 0x39,
    FNEG        = 0x3A,
    FDUP        = 0x3B,
    INT_TO_FP   = 0x3C,
    FP_TO_INT   = 0x3D,

    HALT        = 0xFF
};

// Object system for simple OOP
struct VMObject {
    std::string className;
    std::unordered_map<std::string, int32_t> fields;
    
    VMObject(const std::string& name) : className(name) {}
};

// Call frame for function calls
struct CallFrame {
    size_t return_address;
    size_t base_pointer;
    
    CallFrame(size_t ret, size_t bp) 
        : return_address(ret), base_pointer(bp) {}
};

class VirtualMachine {
public:
    VirtualMachine();
    ~VirtualMachine();
    
    // Load bytecode
    bool loadBytecode(const std::vector<uint8_t>& code);
    bool loadFromFile(const std::string& filename);
    
    // Execution
    void run();
    void step();  // Execute single instruction
    void reset();
    
    // Debug features
    void setDebugMode(bool enabled) { debug_mode = enabled; }
    void dumpStack() const;
    void dumpMemory() const;
    void disassemble() const;
    
    // Statistics
    void printStats() const;
    
    // Get status
    bool isHalted() const { return halted; }
    bool hasError() const { return error_flag; }
    std::string getError() const { return error_message; }
    
private:
    // Bytecode and execution state
    std::vector<uint8_t> bytecode;
    size_t instruction_pointer;
    bool halted;
    bool error_flag;
    std::string error_message;
    bool debug_mode;
    
    // Runtime data structures
    std::vector<int32_t> stack;              // Main operand stack
    std::vector<int32_t> memory;             // Static memory for variables
    std::vector<CallFrame> call_stack;       // Function call frames
    size_t base_pointer;                     // Current base pointer
    
    // Heap management
    std::vector<int32_t> heap;               // Dynamic heap memory
    struct HeapBlock {
        size_t start;
        size_t size;
        bool allocated;
    };
    std::vector<HeapBlock> heap_blocks;      // Heap block metadata
    size_t heap_start_addr;                  // Base address for heap
    
    // String table (loaded from bytecode header)
    std::vector<std::string> string_table;
    
    // Object heap
    std::unordered_map<int32_t, std::shared_ptr<VMObject>> objects;
    int32_t next_object_id;
    
    // Comparison flag (for CMP instruction)
    int32_t cmp_flag;
    
    // FPU: x87-style circular register stack
    float fpu_regs[8];
    int fpu_top;              // index of current ST0
    std::vector<float> float_memory;  // separate float memory space
    
    // Statistics
    size_t instruction_count;
    size_t max_stack_size;
    
    // Instruction execution
    void executeInstruction();
    
    // Stack operations
    void push(int32_t value);
    int32_t pop();
    int32_t peek() const;
    
    // Memory operations
    void storeMemory(int32_t addr, int32_t value);
    int32_t loadMemory(int32_t addr);
    
    // Heap operations
    int32_t allocateHeap(size_t size);
    void freeHeap(int32_t addr);
    bool isHeapAddress(int32_t addr) const;
    
    // Reading from bytecode
    uint8_t readByte();
    int32_t readInt32();
    
    // Object operations
    int32_t createObject(const std::string& className);
    void setField(int32_t objId, const std::string& field, int32_t value);
    int32_t getField(int32_t objId, const std::string& field);
    
    // I/O operations (cross-platform)
    void printValue(int32_t value);
    void printString(const std::string& str);
    int32_t inputNumber();
    std::string inputString();
    
    // Error handling
    void error(const std::string& msg);
    
    // FPU operations
    void fpush(float value);
    float fpop();
    float fpeek() const;
    float readFloat32();
    
    // Utility
    std::string opcodeToString(VMOpcode op) const;
};

#endif // VM_H
