#include "vm.h"
#include "logger.h"
#include <iostream>
#include <string>

void printVMHelp() {
    Logger::out() << "Usage: vm [options] <bytecode file>\n"
              << "Options:\n"
              << "  -h, --help            Show this help message\n"
              << "  -d, --debug           Enable debug mode (trace execution)\n"
              << "  -s, --stats           Show execution statistics\n"
              << "  --disassemble         Disassemble bytecode and exit\n"
              << "  --dump-stack          Dump stack after execution\n"
              << "  --dump-memory         Dump memory after execution\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    float version = 1.0;
    bool show_help = false;
    bool debug_mode = false;
    bool show_stats = false;
    bool disassemble_only = false;
    bool dump_stack = false;
    bool dump_memory = false;
    std::string bytecode_file;

    // Command line parsing
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--version") {
            Logger::out() << "GOC Virtual Machine version: " << version << std::endl;
            Logger::out() << "Cross-platform stack-based bytecode interpreter\n";
            Logger::out() << "Platform: ";
#ifdef _WIN32
            Logger::out() << "Windows\n";
#else
            Logger::out() << "Unix/Linux\n";
#endif
            return 0;
        }
        else if (arg == "-h" || arg == "--help") {
            show_help = true;
        } else if (arg == "-d" || arg == "--debug") {
            debug_mode = true;
        } else if (arg == "-s" || arg == "--stats") {
            show_stats = true;
        } else if (arg == "--disassemble") {
            disassemble_only = true;
        } else if (arg == "--dump-stack") {
            dump_stack = true;
        } else if (arg == "--dump-memory") {
            dump_memory = true;
        } else if (arg[0] == '-') {
            Logger::error() << "Unknown option: " << arg << "\n";
            printVMHelp();
            return 1;
        } else {
            bytecode_file = arg;
        }
    }

    if (show_help) {
        printVMHelp();
        return 0;
    }

    if (bytecode_file.empty()) {
        Logger::error() << "Error: No bytecode file specified\n";
        printVMHelp();
        return 1;
    }

    try {
        Logger::setDebugEnabled(debug_mode);
        Logger::setDebugBuffered(debug_mode);
        VirtualMachine vm;
        
        if (debug_mode) {
            Logger::debug() << "=== GOC Virtual Machine ===\n";
            Logger::debug() << "Loading bytecode: " << bytecode_file << "\n\n";
        }

        if (!vm.loadFromFile(bytecode_file)) {
            Logger::flushDebug();
            Logger::error() << "Error: " << vm.getError() << "\n";
            return 1;
        }

        if (disassemble_only) {
            Logger::flushDebug();
            vm.disassemble();
            return 0;
        }

        vm.setDebugMode(debug_mode);

        if (debug_mode) {
            Logger::debug() << "[Starting execution]\n\n";
        }

        vm.run();

        if (vm.hasError()) {
            Logger::flushDebug();
            Logger::error() << "\nExecution failed: " << vm.getError() << "\n";
            return 1;
        }

        if (debug_mode) {
            Logger::debug() << "\n[Execution completed]\n";
            Logger::out() << "\n";
            Logger::flushDebug();
        }

        if (dump_stack) {
            vm.dumpStack();
        }

        if (dump_memory) {
            vm.dumpMemory();
        }

        if (show_stats) {
            vm.printStats();
        }

        return 0;

    } catch (const std::exception& e) {
        Logger::flushDebug();
        Logger::error() << "\n❌ Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        Logger::flushDebug();
        Logger::error() << "\n❌ Unknown error occurred\n";
        return 1;
    }
}
