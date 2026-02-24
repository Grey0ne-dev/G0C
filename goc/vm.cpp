#include "vm.h"
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <limits>

VirtualMachine::VirtualMachine() 
    : instruction_pointer(0), halted(false), error_flag(false),
      debug_mode(false), base_pointer(0), next_object_id(1),
      cmp_flag(0), instruction_count(0), max_stack_size(0),
      fpu_top(0),
      heap_start_addr(10000) {  // Heap starts at address 10000
    memory.resize(1024, 0);  // 1KB initial static memory
    heap.resize(4096, 0);    // 4KB initial heap
    float_memory.resize(1024, 0.0f);
    std::fill(fpu_regs, fpu_regs + 8, 0.0f);
}

VirtualMachine::~VirtualMachine() {
}

bool VirtualMachine::loadBytecode(const std::vector<uint8_t>& code) {
    bytecode = code;
    reset();
    return true;
}

bool VirtualMachine::loadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        error("Failed to open file: " + filename);
        return false;
    }
    
    // Read string table size
    uint32_t str_count = 0;
    file.read(reinterpret_cast<char*>(&str_count), sizeof(str_count));
    
    if (!file) {
        error("Failed to read string table size");
        return false;
    }
    
    // Read string table
    string_table.clear();
    for (uint32_t i = 0; i < str_count; i++) {
        uint32_t len = 0;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!file) {
            error("Failed to read string length");
            return false;
        }
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        if (!file) {
            error("Failed to read string data");
            return false;
        }
        string_table.push_back(str);
    }
    
    // Read bytecode size
    uint32_t code_size = 0;
    file.read(reinterpret_cast<char*>(&code_size), sizeof(code_size));
    
    if (!file) {
        error("Failed to read bytecode size");
        return false;
    }
    
    // Read bytecode
    bytecode.resize(code_size);
    file.read(reinterpret_cast<char*>(bytecode.data()), code_size);
    
    if (!file) {
        error("Failed to read bytecode");
        return false;
    }
    
    reset();
    return true;
}

void VirtualMachine::reset() {
    instruction_pointer = 0;
    halted = false;
    error_flag = false;
    error_message.clear();
    stack.clear();
    call_stack.clear();
    objects.clear();
    base_pointer = 0;
    next_object_id = 1;
    cmp_flag = 0;
    instruction_count = 0;
    max_stack_size = 0;
    fpu_top = 0;
    std::fill(fpu_regs, fpu_regs + 8, 0.0f);
    std::fill(float_memory.begin(), float_memory.end(), 0.0f);
    std::fill(memory.begin(), memory.end(), 0);
}

void VirtualMachine::run() {
    if (debug_mode) {
        std::cout << "Bytecode size: " << bytecode.size() << " bytes\n";
        std::cout << "Starting execution from IP=" << instruction_pointer << "\n\n";
    }
    
    while (!halted && !error_flag) {
        step();
    }
    
    if (error_flag) {
        std::cerr << "\n❌ VM Error: " << error_message << std::endl;
        std::cerr << "Instruction Pointer: " << instruction_pointer << std::endl;
    }
}

void VirtualMachine::step() {
    if (halted || error_flag) return;
    
    if (instruction_pointer >= bytecode.size()) {
        error("Instruction pointer out of bounds");
        return;
    }
    
    if (debug_mode) {
        std::cout << "[" << instruction_pointer << "] ";
    }
    
    executeInstruction();
    instruction_count++;
    
    if (stack.size() > max_stack_size) {
        max_stack_size = stack.size();
    }
}

void VirtualMachine::executeInstruction() {
    uint8_t opcode_byte = readByte();
    VMOpcode op = static_cast<VMOpcode>(opcode_byte);
    
    if (debug_mode) {
        std::cout << opcodeToString(op) << std::endl;
    }
    
    switch (op) {
        case VMOpcode::PUSH: {
            int32_t value = readInt32();
            push(value);
            break;
        }
        
        case VMOpcode::POP:
            pop();
            break;
        
        case VMOpcode::ADD: {
            int32_t b = pop();
            int32_t a = pop();
            push(a + b);
            break;
        }
        
        case VMOpcode::SUB: {
            int32_t b = pop();
            int32_t a = pop();
            push(a - b);
            break;
        }
        
        case VMOpcode::MUL: {
            int32_t b = pop();
            int32_t a = pop();
            push(a * b);
            break;
        }
        
        case VMOpcode::DIV: {
            int32_t b = pop();
            int32_t a = pop();
            if (b == 0) {
                error("Division by zero");
                return;
            }
            push(a / b);
            break;
        }
        
        case VMOpcode::MOD: {
            int32_t b = pop();
            int32_t a = pop();
            if (b == 0) {
                error("Modulo by zero");
                return;
            }
            push(a % b);
            break;
        }
        
        case VMOpcode::DUP:
            push(peek());
            break;
        
        case VMOpcode::SWAP: {
            if (stack.size() < 2) {
                error("Stack underflow in SWAP");
                return;
            }
            int32_t a = pop();
            int32_t b = pop();
            push(a);
            push(b);
            break;
        }
        
        case VMOpcode::PRINT: {
            int32_t value = pop();
            printValue(value);
            break;
        }
        
        case VMOpcode::PRINT_STR: {
            int32_t str_id = pop();
            if (str_id >= 0 && static_cast<size_t>(str_id) < string_table.size()) {
                printString(string_table[str_id]);
            } else {
                error("Invalid string ID");
            }
            break;
        }
        
        case VMOpcode::INPUT: {
            int32_t value = inputNumber();
            push(value);
            break;
        }
        
        case VMOpcode::INPUT_STR: {
            std::string str = inputString();
            // Store string in table and push ID
            string_table.push_back(str);
            push(static_cast<int32_t>(string_table.size() - 1));
            break;
        }
        
        case VMOpcode::PUSH_STR: {
            int32_t str_id = readInt32();
            push(str_id);
            break;
        }
        
        case VMOpcode::JMP: {
            int32_t addr = readInt32();
            instruction_pointer = addr;
            break;
        }
        
        case VMOpcode::JZ: {
            int32_t addr = readInt32();
            int32_t value = pop();
            if (value == 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::JNZ: {
            int32_t addr = readInt32();
            int32_t value = pop();
            if (value != 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::CMP: {
            int32_t b = pop();
            int32_t a = pop();
            cmp_flag = (a < b) ? -1 : (a > b) ? 1 : 0;
            break;
        }
        
        case VMOpcode::JL: {
            int32_t addr = readInt32();
            if (cmp_flag < 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::JG: {
            int32_t addr = readInt32();
            if (cmp_flag > 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::JLE: {
            int32_t addr = readInt32();
            if (cmp_flag <= 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::JGE: {
            int32_t addr = readInt32();
            if (cmp_flag >= 0) {
                instruction_pointer = addr;
            }
            break;
        }
        
        case VMOpcode::CALL: {
            int32_t addr = readInt32();
            call_stack.emplace_back(instruction_pointer, base_pointer);
            instruction_pointer = addr;
            break;
        }
        
        case VMOpcode::RET: {
            if (call_stack.empty()) {
                error("Return without call");
                return;
            }
            CallFrame frame = call_stack.back();
            call_stack.pop_back();
            instruction_pointer = frame.return_address;
            base_pointer = frame.base_pointer;
            break;
        }
        
        case VMOpcode::PUSH_BP:
            push(static_cast<int32_t>(base_pointer));
            base_pointer = stack.size();
            break;
        
        case VMOpcode::POP_BP:
            // Restore BP from saved location at stack[BP-1]
            if (base_pointer == 0 || base_pointer > stack.size()) {
                error("Invalid base pointer in POP_BP");
                return;
            }
            base_pointer = static_cast<size_t>(stack[base_pointer - 1]);
            break;
        
        case VMOpcode::LOAD: {
            int32_t addr = readInt32();
            int32_t value = loadMemory(addr);
            if (debug_mode) {
                std::cerr << "LOAD addr=" << addr << " value=" << value << "\n";
            }
            push(value);
            break;
        }
        
        case VMOpcode::STORE: {
            int32_t addr = pop();
            int32_t value = pop();
            if (debug_mode) {
                std::cerr << "STORE addr=" << addr << " value=" << value << "\n";
            }
            storeMemory(addr, value);
            break;
        }
        
        case VMOpcode::LOAD_BP: {
            int32_t offset = readInt32();
            // Handle negative offsets correctly (for parameters)
            int64_t addr = static_cast<int64_t>(base_pointer) + static_cast<int64_t>(offset);
            if (addr < 0 || addr >= static_cast<int64_t>(stack.size())) {
                std::cerr << "DEBUG: LOAD_BP offset=" << offset << " BP=" << base_pointer 
                         << " addr=" << addr << " stack_size=" << stack.size() << "\n";
                error("BP-relative load out of bounds");
                return;
            }
            int32_t value = stack[static_cast<size_t>(addr)];
            if (debug_mode) {
                std::cerr << "LOAD_BP offset=" << offset << " BP=" << base_pointer 
                         << " addr=" << addr << " value=" << value << "\n";
            }
            push(value);
            break;
        }
        
        case VMOpcode::STORE_BP: {
            int32_t offset = readInt32();
            int32_t value = pop();
            // Handle negative offsets correctly (for parameters)
            int64_t addr = static_cast<int64_t>(base_pointer) + static_cast<int64_t>(offset);
            if (addr < 0) {
                error("BP-relative store out of bounds (negative address)");
                return;
            }
            if (static_cast<size_t>(addr) >= stack.size()) {
                stack.resize(static_cast<size_t>(addr) + 1, 0);
            }
            stack[static_cast<size_t>(addr)] = value;
            break;
        }
        
        case VMOpcode::LOAD_INDIRECT: {
            int32_t addr = pop();
            int32_t value = loadMemory(addr);
            if (debug_mode) {
                std::cerr << "LOAD_INDIRECT addr=" << addr << " value=" << value << "\n";
            }
            push(value);
            break;
        }
        
        case VMOpcode::STORE_INDIRECT: {
            int32_t addr = pop();
            int32_t value = pop();
            if (debug_mode) {
                std::cerr << "STORE_INDIRECT addr=" << addr << " value=" << value << "\n";
            }
            storeMemory(addr, value);
            break;
        }
        
        case VMOpcode::ALLOC: {
            int32_t size = pop();
            if (size <= 0) {
                error("Invalid allocation size");
                return;
            }
            int32_t addr = allocateHeap(static_cast<size_t>(size));
            if (addr < 0) {
                error("Heap allocation failed");
                return;
            }
            push(addr);
            break;
        }
        
        case VMOpcode::FREE: {
            int32_t addr = pop();
            if (addr < 0) {
                error("Invalid address for free");
                return;
            }
            freeHeap(addr);
            break;
        }
        
        // --- FPU instructions ---
        case VMOpcode::FPUSH: {
            float val = readFloat32();
            fpush(val);
            break;
        }
        
        case VMOpcode::FPOP:
            fpop();
            break;
        
        case VMOpcode::FADD: {
            float b = fpop();
            float a = fpop();
            fpush(a + b);
            break;
        }
        
        case VMOpcode::FSUB: {
            float b = fpop();
            float a = fpop();
            fpush(a - b);
            break;
        }
        
        case VMOpcode::FMUL: {
            float b = fpop();
            float a = fpop();
            fpush(a * b);
            break;
        }
        
        case VMOpcode::FDIV: {
            float b = fpop();
            float a = fpop();
            if (b == 0.0f) {
                error("FPU division by zero");
                return;
            }
            fpush(a / b);
            break;
        }
        
        case VMOpcode::FLOAD: {
            int32_t addr = readInt32();
            if (addr < 0) { error("Negative FPU memory address"); return; }
            if (static_cast<size_t>(addr) >= float_memory.size()) {
                error("FPU memory access out of bounds");
                return;
            }
            fpush(float_memory[static_cast<size_t>(addr)]);
            break;
        }
        
        case VMOpcode::FSTORE: {
            int32_t addr = readInt32();
            float val = fpop();
            if (addr < 0) { error("Negative FPU memory address"); return; }
            if (static_cast<size_t>(addr) >= float_memory.size()) {
                float_memory.resize(static_cast<size_t>(addr) + 256, 0.0f);
            }
            float_memory[static_cast<size_t>(addr)] = val;
            break;
        }
        
        case VMOpcode::FPRINT: {
            float val = fpop();
            std::cout << val;
            break;
        }
        
        case VMOpcode::FCMP: {
            float b = fpop();
            float a = fpop();
            cmp_flag = (a < b) ? -1 : (a > b) ? 1 : 0;
            break;
        }
        
        case VMOpcode::FNEG: {
            float val = fpop();
            fpush(-val);
            break;
        }
        
        case VMOpcode::FDUP: {
            float val = fpeek();
            fpush(val);
            break;
        }
        
        case VMOpcode::INT_TO_FP: {
            int32_t ival = pop();
            fpush(static_cast<float>(ival));
            break;
        }
        
        case VMOpcode::FP_TO_INT: {
            float fval = fpop();
            push(static_cast<int32_t>(fval));
            break;
        }
        
        case VMOpcode::HALT:
            halted = true;
            break;
        
        default:
            error("Unknown opcode: 0x" + std::to_string(opcode_byte));
            break;
    }
}

void VirtualMachine::push(int32_t value) {
    stack.push_back(value);
}

int32_t VirtualMachine::pop() {
    if (stack.empty()) {
        error("Stack underflow");
        return 0;
    }
    int32_t value = stack.back();
    stack.pop_back();
    return value;
}

int32_t VirtualMachine::peek() const {
    if (stack.empty()) {
        return 0;
    }
    return stack.back();
}

void VirtualMachine::storeMemory(int32_t addr, int32_t value) {
    if (addr < 0) {
        error("Negative memory address");
        return;
    }
    
    // Check if it's a heap address
    if (isHeapAddress(addr)) {
        size_t heap_offset = static_cast<size_t>(addr - heap_start_addr);
        if (heap_offset >= heap.size()) {
            heap.resize(heap_offset + 1024, 0);
        }
        heap[heap_offset] = value;
    } else {
        // Static memory
        if (static_cast<size_t>(addr) >= memory.size()) {
            memory.resize(addr + 1024, 0);
        }
        memory[addr] = value;
    }
}

int32_t VirtualMachine::loadMemory(int32_t addr) {
    if (addr < 0) {
        error("Negative memory address");
        return 0;
    }
    
    // Check if it's a heap address
    if (isHeapAddress(addr)) {
        size_t heap_offset = static_cast<size_t>(addr - heap_start_addr);
        if (heap_offset >= heap.size()) {
            error("Heap memory access out of bounds");
            return 0;
        }
        return heap[heap_offset];
    } else {
        // Static memory
        if (static_cast<size_t>(addr) >= memory.size()) {
            error("Memory access out of bounds");
            return 0;
        }
        return memory[addr];
    }
}

int32_t VirtualMachine::allocateHeap(size_t size) {
    // First-fit allocation strategy
    for (auto& block : heap_blocks) {
        if (!block.allocated && block.size >= size) {
            // Found a free block that fits
            if (block.size > size) {
                // Split the block
                HeapBlock new_block;
                new_block.start = block.start + size;
                new_block.size = block.size - size;
                new_block.allocated = false;
                heap_blocks.push_back(new_block);
                block.size = size;
            }
            block.allocated = true;
            
            // Ensure heap is large enough
            if (block.start + block.size > heap.size()) {
                heap.resize(block.start + block.size + 1024, 0);
            }
            
            return static_cast<int32_t>(heap_start_addr + block.start);
        }
    }
    
    // No suitable free block found, allocate at the end
    size_t new_start = 0;
    if (!heap_blocks.empty()) {
        auto& last = heap_blocks.back();
        new_start = last.start + last.size;
    }
    
    HeapBlock new_block;
    new_block.start = new_start;
    new_block.size = size;
    new_block.allocated = true;
    
    // Ensure heap is large enough
    if (new_block.start + new_block.size > heap.size()) {
        heap.resize(new_block.start + new_block.size + 1024, 0);
    }
    
    heap_blocks.push_back(new_block);
    return static_cast<int32_t>(heap_start_addr + new_start);
}

void VirtualMachine::freeHeap(int32_t addr) {
    if (!isHeapAddress(addr)) {
        error("Attempting to free non-heap address");
        return;
    }
    
    size_t heap_offset = static_cast<size_t>(addr - heap_start_addr);
    
    // Find the block
    for (auto& block : heap_blocks) {
        if (block.start == heap_offset && block.allocated) {
            block.allocated = false;
            
            // Zero out the freed memory
            for (size_t i = 0; i < block.size; i++) {
                if (block.start + i < heap.size()) {
                    heap[block.start + i] = 0;
                }
            }
            return;
        }
    }
    
    error("Invalid heap address for free operation");
}

bool VirtualMachine::isHeapAddress(int32_t addr) const {
    return addr >= static_cast<int32_t>(heap_start_addr);
}

uint8_t VirtualMachine::readByte() {
    if (instruction_pointer >= bytecode.size()) {
        error("Unexpected end of bytecode");
        return 0;
    }
    uint8_t byte = bytecode[instruction_pointer++];
    if (debug_mode) {
        std::cerr << "  readByte at " << (instruction_pointer-1) << ": 0x" 
                  << std::hex << static_cast<int>(byte) << std::dec << std::endl;
    }
    return byte;
}

int32_t VirtualMachine::readInt32() {
    if (instruction_pointer + 4 > bytecode.size()) {
        error("Unexpected end of bytecode reading int32");
        return 0;
    }
    
    int32_t value;
    std::memcpy(&value, &bytecode[instruction_pointer], sizeof(int32_t));
    instruction_pointer += 4;
    return value;
}

int32_t VirtualMachine::createObject(const std::string& className) {
    int32_t id = next_object_id++;
    objects[id] = std::make_shared<VMObject>(className);
    return id;
}

void VirtualMachine::setField(int32_t objId, const std::string& field, int32_t value) {
    auto it = objects.find(objId);
    if (it == objects.end()) {
        error("Invalid object ID");
        return;
    }
    it->second->fields[field] = value;
}

int32_t VirtualMachine::getField(int32_t objId, const std::string& field) {
    auto it = objects.find(objId);
    if (it == objects.end()) {
        error("Invalid object ID");
        return 0;
    }
    auto field_it = it->second->fields.find(field);
    if (field_it == it->second->fields.end()) {
        return 0;  // Default value
    }
    return field_it->second;
}

// Cross-platform I/O implementations
void VirtualMachine::printValue(int32_t value) {
    std::cout << value;
}

void VirtualMachine::printString(const std::string& str) {
    std::cout << str;
}

int32_t VirtualMachine::inputNumber() {
    int32_t value;
    std::cin >> value;
    
    // Clear input buffer on all platforms
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return 0;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    return value;
}

std::string VirtualMachine::inputString() {
    std::string str;
    std::getline(std::cin, str);
    return str;
}

void VirtualMachine::error(const std::string& msg) {
    error_flag = true;
    error_message = msg;
    halted = true;
}

// FPU circular stack (x87-style, 8 slots)
void VirtualMachine::fpush(float value) {
    fpu_top = (fpu_top - 1 + 8) % 8;
    fpu_regs[fpu_top] = value;
}

float VirtualMachine::fpop() {
    float val = fpu_regs[fpu_top];
    fpu_regs[fpu_top] = 0.0f;
    fpu_top = (fpu_top + 1) % 8;
    return val;
}

float VirtualMachine::fpeek() const {
    return fpu_regs[fpu_top];
}

float VirtualMachine::readFloat32() {
    if (instruction_pointer + 4 > bytecode.size()) {
        error("Unexpected end of bytecode reading float32");
        return 0.0f;
    }
    float value;
    std::memcpy(&value, &bytecode[instruction_pointer], sizeof(float));
    instruction_pointer += 4;
    return value;
}

void VirtualMachine::dumpStack() const {
    std::cout << "\n=== Stack Dump ===" << std::endl;
    std::cout << "Size: " << stack.size() << std::endl;
    
    if (stack.empty()) {
        std::cout << "(empty)" << std::endl;
        return;
    }
    
    for (int i = static_cast<int>(stack.size()) - 1; i >= 0; i--) {
        std::cout << "[" << i << "] " << stack[i];
        if (i == static_cast<int>(base_pointer)) {
            std::cout << " <-- BP";
        }
        std::cout << std::endl;
    }
}

void VirtualMachine::dumpMemory() const {
    std::cout << "\n=== Memory Dump ===" << std::endl;
    bool has_data = false;
    
    for (size_t i = 0; i < memory.size(); i++) {
        if (memory[i] != 0) {
            if (!has_data) {
                has_data = true;
            }
            std::cout << "[" << i << "] = " << memory[i] << std::endl;
        }
    }
    
    if (!has_data) {
        std::cout << "(all zeros)" << std::endl;
    }
}

void VirtualMachine::disassemble() const {
    std::cout << "\n=== Bytecode Disassembly ===" << std::endl;
    std::cout << "Bytecode size: " << bytecode.size() << " bytes" << std::endl;
    std::cout << "First 10 bytes (hex): ";
    for (size_t i = 0; i < std::min(size_t(10), bytecode.size()); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i]) << " ";
    }
    std::cout << std::dec << std::endl << std::endl;
    
    size_t ip = 0;
    while (ip < bytecode.size()) {
        std::cout << std::setw(6) << ip << ": ";
        
        uint8_t opcode = bytecode[ip++];
        VMOpcode op = static_cast<VMOpcode>(opcode);
        
        std::cout << opcodeToString(op);
        
        // Print operands for instructions that have them
        switch (op) {
            case VMOpcode::PUSH:
            case VMOpcode::JMP:
            case VMOpcode::JZ:
            case VMOpcode::JNZ:
            case VMOpcode::JL:
            case VMOpcode::JG:
            case VMOpcode::JLE:
            case VMOpcode::JGE:
            case VMOpcode::CALL:
            case VMOpcode::LOAD:
            case VMOpcode::LOAD_BP:
            case VMOpcode::STORE_BP:
            case VMOpcode::PUSH_STR:
            case VMOpcode::FLOAD:
            case VMOpcode::FSTORE:
                if (ip + 4 <= bytecode.size()) {
                    int32_t value;
                    std::memcpy(&value, &bytecode[ip], sizeof(int32_t));
                    std::cout << " " << value;
                    ip += 4;
                }
                break;
            case VMOpcode::FPUSH:
                if (ip + 4 <= bytecode.size()) {
                    float fvalue;
                    std::memcpy(&fvalue, &bytecode[ip], sizeof(float));
                    std::cout << " " << fvalue;
                    ip += 4;
                }
                break;
            default:
                break;
        }
        
        std::cout << std::endl;
    }
}

void VirtualMachine::printStats() const {
    std::cout << "\n=== VM Statistics ===" << std::endl;
    std::cout << "Instructions executed: " << instruction_count << std::endl;
    std::cout << "Max stack depth: " << max_stack_size << std::endl;
    std::cout << "Objects created: " << (next_object_id - 1) << std::endl;
    std::cout << "Static memory allocated: " << memory.size() << " cells" << std::endl;
    std::cout << "Heap size: " << heap.size() << " cells" << std::endl;
    std::cout << "Heap blocks: " << heap_blocks.size() << " (";
    size_t allocated_blocks = 0;
    for (const auto& block : heap_blocks) {
        if (block.allocated) allocated_blocks++;
    }
    std::cout << allocated_blocks << " allocated, " 
              << (heap_blocks.size() - allocated_blocks) << " free)" << std::endl;
}

std::string VirtualMachine::opcodeToString(VMOpcode op) const {
    switch (op) {
        case VMOpcode::PUSH: return "PUSH";
        case VMOpcode::POP: return "POP";
        case VMOpcode::ADD: return "ADD";
        case VMOpcode::SUB: return "SUB";
        case VMOpcode::MUL: return "MUL";
        case VMOpcode::DIV: return "DIV";
        case VMOpcode::MOD: return "MOD";
        case VMOpcode::DUP: return "DUP";
        case VMOpcode::SWAP: return "SWAP";
        case VMOpcode::PRINT: return "PRINT";
        case VMOpcode::PRINT_STR: return "PRINT_STR";
        case VMOpcode::INPUT: return "INPUT";
        case VMOpcode::INPUT_STR: return "INPUT_STR";
        case VMOpcode::JMP: return "JMP";
        case VMOpcode::JZ: return "JZ";
        case VMOpcode::JNZ: return "JNZ";
        case VMOpcode::CMP: return "CMP";
        case VMOpcode::JL: return "JL";
        case VMOpcode::JG: return "JG";
        case VMOpcode::JLE: return "JLE";
        case VMOpcode::JGE: return "JGE";
        case VMOpcode::CALL: return "CALL";
        case VMOpcode::RET: return "RET";
        case VMOpcode::LOAD: return "LOAD";
        case VMOpcode::STORE: return "STORE";
        case VMOpcode::LOAD_BP: return "LOAD_BP";
        case VMOpcode::STORE_BP: return "STORE_BP";
        case VMOpcode::PUSH_BP: return "PUSH_BP";
        case VMOpcode::POP_BP: return "POP_BP";
        case VMOpcode::PUSH_STR: return "PUSH_STR";
        case VMOpcode::LOAD_INDIRECT: return "LOAD_INDIRECT";
        case VMOpcode::STORE_INDIRECT: return "STORE_INDIRECT";
        case VMOpcode::ALLOC: return "ALLOC";
        case VMOpcode::FREE: return "FREE";
        case VMOpcode::FPUSH: return "FPUSH";
        case VMOpcode::FPOP: return "FPOP";
        case VMOpcode::FADD: return "FADD";
        case VMOpcode::FSUB: return "FSUB";
        case VMOpcode::FMUL: return "FMUL";
        case VMOpcode::FDIV: return "FDIV";
        case VMOpcode::FLOAD: return "FLOAD";
        case VMOpcode::FSTORE: return "FSTORE";
        case VMOpcode::FPRINT: return "FPRINT";
        case VMOpcode::FCMP: return "FCMP";
        case VMOpcode::FNEG: return "FNEG";
        case VMOpcode::FDUP: return "FDUP";
        case VMOpcode::INT_TO_FP: return "INT_TO_FP";
        case VMOpcode::FP_TO_INT: return "FP_TO_INT";
        case VMOpcode::HALT: return "HALT";
        default: return "UNKNOWN";
    }
}
