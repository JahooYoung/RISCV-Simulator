#include <getopt.h>
#include <string>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "simulator.hpp"
using namespace std;
using nlohmann::json;

void print_help(string name)
{
    cerr << "Usage: " + name + " [options] elf_file" << endl;
    cerr << "Options:" << endl;
    cerr << "  -c, --config config_file Specify the configuration file," << endl;
    cerr << "                           default is 'default_config.json'" << endl;
    cerr << "  -s                       Single step mode" << endl;
    cerr << "  -i, --info info_file     Output filename of Elf information" << endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    // handle command line arguments
    static option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"info",   required_argument, 0, 'i'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    string config_filename = "default_config.json";
    json config_patch;
    string info_filename;
    string elf_filename;

    while ((opt =
        getopt_long(argc, argv, "si:c:", long_options, &option_index)) != -1) {

        switch (opt) {
        case 'c':
            config_filename = optarg;
            break;
        case 'i':
            config_patch["info_file"] = string(optarg);
            break;
        case 's':
            config_patch["single_step"] = true;
            break;
        default:
            print_help(argv[0]);
        }
    }
    if (optind >= argc) {
        cerr << "error: No ELF file provided" << endl << endl;
        print_help(argv[0]);
    }
    config_patch["elf_file"] = argv[optind];

    // read config file
    ifstream config_file(config_filename);
    if (!config_file) {
        cerr << "error: cannot open " << config_filename << endl;
        exit(EXIT_FAILURE);
    }
    json config;
    config_file >> config;
    config.merge_patch(config_patch);

    Simulator simulator(config);
    simulator.run();


    return 0;
}
