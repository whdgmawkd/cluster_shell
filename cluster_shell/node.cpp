#include <chrono>
#include <cstdio>
#include <cstring>
#include <error.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>

#include "node.h"

data_buffer::data_buffer(size_t buffer_size)
    : buffer_size(buffer_size), data_length(0) {
    buffer = new char[buffer_size];
}

bool data_buffer::append(char c) {
    if (data_length == buffer_size) {
        throw node_error("PIPE Data Buffer Overflow");
    }
    if (c == '\n') { // isspace(c) != 0) {
        return data_length > 0;
    }
    buffer[data_length++] = c;
    return false;
}

std::string data_buffer::to_string() {
    if (data_length == 0) {
        return "";
    }
    std::string ret(buffer, buffer + data_length);
    data_length = 0;
    return ret;
}

node_error::node_error(std::string message)
    : error_message(message) {
}

const char *node_error::what() const noexcept {
    return error_message.c_str();
}

ssh_node::ssh_node(std::string hostname, std::vector<std::string> command, std::string username, std::string password)
    : hostname(hostname), command(command), username(username), password(password), pid(-1) {

    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        throw node_error(hostname + ": PIPE Failed. " + strerror(errno));
    }

    std::string command_string = " ";
    for (std::string &cmd : command) {
        command_string += cmd + " ";
    }

    std::vector<std::string> expect_script = {
        "/bin/expect",
        "-c", "log_user 0",
        "-c", "spawn ssh -l " + username + " " + hostname + command_string, // + " cat /proc/cmdline",
        "-c", "expect -re \"password\"",
        "-c", "send \"" + password + "\r\"",
        "-c", "log_user 1",
        "-c", "interact"};

    std::vector<char *> argv;

    for (std::string &cmd : expect_script) {
        argv.push_back(string_to_cstr(cmd));
    }

    argv.push_back(nullptr);

    pid = fork();
    // run on child process
    if (pid == 0) {
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[1]);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[1]);
        close(stderr_pipe[0]);
        setvbuf(stdin, nullptr, _IONBF, 0);
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        if (execv(argv[0], &argv[0]) != 0) {
            throw node_error(hostname + " : execv failed. " + strerror(errno));
        }
    }
    // spawn failed
    if (pid == -1) {
        throw node_error(hostname + " : fork failed. " + strerror(errno));
    }
    // parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stdout_pipe[1]);

    fd.stdin_fd = stdin_pipe[1];
    fd.stdout_fd = stdout_pipe[0];
    fd.stderr_fd = stderr_pipe[0];

    fcntl(fd.stdout_fd, F_SETFL, O_NONBLOCK);
    fcntl(fd.stderr_fd, F_SETFL, O_NONBLOCK);
}

ssh_node::~ssh_node() {
    int status_code = 0;
    waitpid(pid, &status_code, 0);
    if (WEXITSTATUS(status_code) != 0) {
    }
    std::cout << hostname << " : clsh node exited with status code (" << WEXITSTATUS(status_code) << ")" << std::endl;
    pid = -1;
    close(fd.stdin_fd);
    close(fd.stdout_fd);
    close(fd.stderr_fd);
}

void ssh_node::send_input(std::string text) {
    if (pid == -1)
        return;
    text += "\n";
    if (write(fd.stdin_fd, text.c_str(), text.length()) == -1) {
        throw node_error(hostname + " : stdin write failed. " + strerror(errno));
    }
}

char *string_to_cstr(std::string str) {
    char *cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());
    return cstr;
}

std::string ssh_node::get_hostname() {
    return hostname;
}