#include <getopt.h>
#include <string>
#include <iostream>
#include <fstream>
#include <utility>
#include <yaml-cpp/yaml.h>
#include "simulator.hpp"
using namespace std;

void print_help_and_exit(string name)
{
    cerr << "Usage: " + name + " [options] elf_file|trace_file [args...]" << endl << endl;
    cerr << "Options:" << endl;
    cerr << "  -h, --help               Print this help" << endl;
    cerr << "  -c, --config config_file Specify the configuration file," << endl;
    cerr << "                           default is 'default_config.yml'" << endl;
    cerr << "Options for elf_file:" << endl;
    cerr << "  -s                       Single step mode" << endl;
    cerr << "  -i, --info info_file     Output filename of Elf information" << endl;
    cerr << "  -v                       Verbose mode" << endl;
    cerr << endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    // handle command line arguments
    static option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"info",   required_argument, 0, 'i'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    string config_filename = "default_config.yml";
    YAML::Node option;

    while ((opt =
        getopt_long(argc, argv, "svi:c:h", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'c':
            config_filename = optarg;
            break;
        case 'i':
            option["info_file"] = string(optarg);
            break;
        case 's':
            option["single_step"] = true;
            break;
        case 'v':
            option["verbose"] = true;
            break;
        case 'h':
        default:
            print_help_and_exit(argv[0]);
        }
    }
    if (optind >= argc) {
        cerr << "error: No ELF file provided" << endl << endl;
        print_help_and_exit(argv[0]);
    }
    string elf_file;
    option["elf_file"] = elf_file = argv[optind];

    ArgumentVector args;
    for (; optind < argc; optind++)
        args.push_back(argv[optind]);

    // read config file
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_filename);
    } catch (const YAML::BadFile&) {
        cerr << "error: cannot open " << config_filename << endl;
        exit(EXIT_FAILURE);
    } catch (const YAML::ParserException& err) {
        cerr << config_filename << ": " << err.what() << endl;
        exit(EXIT_FAILURE);
    }

    if (elf_file.size() >= 6 && elf_file.substr(elf_file.size() - 6) == ".trace") {
        MemorySystem mem_sys(config["cache"], config["memory_cycles"].as<int>(100));
        mem_sys.run_trace(elf_file);
    } else {
        Simulator simulator(option, config, move(args));
        simulator.start();
    }

    return 0;
}
