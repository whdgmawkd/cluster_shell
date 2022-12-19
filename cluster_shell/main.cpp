#include <cstdlib>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "event_loop.h"
#include "node.h"

#define OPT_HOSTNAME 'h'
#define OPT_INTERACTIVE 'i'
#define OPT_HOSTFILE 1000
#define OPT_OUT 1001
#define OPT_ERR 1002

enum OPT {
    HOSTS = 'h',
    INTERACTIVE = 'i',
    HOSTFILE = 1000,
    OUT = 1001,
    ERR = 1002
};

const static char ENV_HOSTS[] = "CLSH_HOSTS";
const static char ENV_HOSTFILE[] = "CLSH_HOSTFILE";

const static char FILE_HOSTFILE[] = ".hostfile";

/**
 * Arguments
 */
static struct option long_options[] = {
    {"hosts", required_argument, nullptr, OPT::HOSTS},
    {"hostfile", required_argument, nullptr, OPT::HOSTFILE},
    {"out", required_argument, nullptr, OPT::OUT},
    {"err", required_argument, nullptr, OPT::ERR},
    {"interactive", no_argument, nullptr, OPT::INTERACTIVE},
    {nullptr, 0, nullptr, 0}};

std::vector<std::string> split(std::string string, char delimeter) {
    std::stringstream ss(string);
    std::string tmp;
    std::vector<std::string> result;
    while (std::getline(ss, tmp, delimeter)) {
        result.push_back(tmp);
    }
    return result;
}

int main(int argc, char **argv) {
    int next_opt;
    std::vector<std::string> hosts;
    std::string hostfile = FILE_HOSTFILE;
    bool is_interactive = false;
    bool arg_hosts = false, arg_hostfile = false, arg_out = false, arg_err = false;
    std::string out_path;
    std::string err_path;
    while ((next_opt = getopt_long(argc, argv, "h:i", long_options, nullptr)) != -1) {
        switch (next_opt) {
        case OPT::HOSTS:
            arg_hosts = true;
            std::cout << "using -h option" << std::endl;
            hosts = split(optarg, ',');
            break;
        case OPT::INTERACTIVE:
            std::cout << "using -i option" << std::endl;
            is_interactive = true;
            break;
        case OPT::HOSTFILE:
            arg_hostfile = true;
            std::cout << "using --hostfile option" << std::endl;
            hostfile = optarg;
            break;
        case OPT::OUT:
            arg_out = true;
            std::cout << "using --out option" << std::endl;
            out_path = optarg;
            break;
        case OPT::ERR:
            arg_err = true;
            std::cout << "using --err option" << std::endl;
            err_path = optarg;
            break;
        case '?':
            break;
        default:
            std::cout << "Unknown Argument" << std::endl;
            exit(-1);
        }
    }
    std::vector<std::string> commands;
    for (; optind < argc; optind++) {
        commands.push_back(argv[optind]);
    }
    if (!arg_hosts && !arg_hostfile) {
        if (getenv(ENV_HOSTS)) {
            std::cout << "Note: use CLSH_HOSTS=" << getenv(ENV_HOSTS) << std::endl;
            hosts = split(getenv(ENV_HOSTS), ':');
        } else if (getenv(ENV_HOSTFILE)) {
            std::cout << "Note: use CLSH_HOSTFILE=" << getenv(ENV_HOSTFILE) << std::endl;
            hostfile = getenv(ENV_HOSTFILE);
        } else if (stat(FILE_HOSTFILE, nullptr) == 0) {
            std::cout << "Note: use hostfile '" << FILE_HOSTFILE << "'" << std::endl;
        } else {
            std::cout << "--hostfile 옵션이 제공되지 않았습니다." << std::endl;
            exit(-1);
        }
    }
    signal(SIGPIPE, SIG_IGN);

    event_loop loop(arg_out, out_path, arg_err, err_path, is_interactive, commands);
    std::vector<ssh_node *> nodes;
    for (std::string &host : hosts) {
        loop.create_node(host, "ubuntu", "ubuntu");
    }
    loop.start_event_loop();
    loop.wait_event_loop();
    loop.stop_event_loop();

    return 0;
}