#ifndef __NODE_H__
#define __NODE_H__

#include <exception>
#include <mutex>
#include <queue>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

#include "constant.h"

/**
 * Remote Node
 */

using node_output = std::pair<output_type, std::string>;

struct node_filedes_t {
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
};

class data_buffer {
  public:
    size_t buffer_size;
    size_t data_length;
    char *buffer;
    data_buffer(size_t buffer_size = 4096);
    bool append(char c);
    std::string to_string();
};

class node_error : public std::exception {
  private:
    std::string error_message;

  public:
    node_error(std::string message);
    const char *what() const noexcept override;
};

class ssh_node {
  private:
    std::string hostname;
    std::string username;
    std::string password;
    pid_t pid;

    // std::mutex output_queue_lock;

    // std::queue<node_output> output;
    std::vector<std::string> command;

    // bool is_file_output;
    // std::string file_output_path;

    // bool interactive_mode;

    // std::thread stdout_task, stderr_task;

    // void stdout_pipe();
    // void stderr_pipe();

  public:
    data_buffer stdout_buffer;
    data_buffer stderr_buffer;
    node_filedes_t fd;

    ssh_node(std::string hostname, std::vector<std::string> command, std::string username = "ubuntu", std::string password = "ubuntu");
    ~ssh_node();
    // bool is_wait_stdin();
    void send_input(std::string text);
    // node_output get_output();
    std::string get_hostname();
};

char *string_to_cstr(std::string);

#endif