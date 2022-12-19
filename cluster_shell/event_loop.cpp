#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>

#include "event_loop.h"

input_event::input_event(input_type t, std::string c, std::string h) {
    type = t;
    command = c;
    hostname = h;
}

event_loop::event_loop(bool use_file_stdout, std::string file_stdout_dir, bool use_file_stderr, std::string file_stderr_dir, bool interactive_mode, std::vector<std::string> commands)
    : use_file_stdout(use_file_stdout), file_stdout_dir(file_stdout_dir), use_file_stderr(use_file_stderr), file_stderr_dir(file_stderr_dir), interactive_mode(interactive_mode), commands(commands) {
    running = false;
    epoll_fd = epoll_create(1);
    if (use_file_stdout && mkdir(file_stdout_dir.c_str(), 0755) == -1) {
        std::cout << "stdout_dir create failed : " << strerror(errno) << std::endl;
        exit(-1);
    }
    if (use_file_stderr && mkdir(file_stderr_dir.c_str(), 0755) == -1) {
        std::cout << "stderr_dir create failed : " << strerror(errno) << std::endl;
    }
}

void event_loop::start_event_loop() {
    running = true;
    if (interactive_mode)
        input_handler_task = std::thread([&] { this->input_handler(); });
    output_handler_task = std::thread([&] { this->output_handler(); });
    event_loop_task = std::thread([&] { this->loop_handler(); });
}

void event_loop::wait_event_loop() {
    if (event_loop_task.joinable()) {
        event_loop_task.join();
    }
}

void event_loop::stop_event_loop() {
    running = false;
    if (input_handler_task.joinable()) {
        input_handler_task.join();
    }
    if (output_handler_task.joinable()) {
        output_handler_task.join();
    }
    if (event_loop_task.joinable()) {
        event_loop_task.join();
    }
}

bool event_loop::create_node(std::string hostname, std::string username, std::string password) {
    ssh_node *new_node = new ssh_node(hostname, commands, username, password);
    node_filedes_t fd = new_node->fd;

    // epoll
    epoll_event out, err;
    out.data.fd = fd.stdout_fd;
    out.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    err.data.fd = fd.stderr_fd;
    err.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd.stdout_fd, &out) != 0 || epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd.stderr_fd, &err) != 0) {
        delete new_node;
        return false;
    }

    fd_map[fd.stdout_fd].buffer = &new_node->stdout_buffer;
    fd_map[fd.stdout_fd].type = output_type::STDOUT;
    fd_map[fd.stdout_fd].hostname = hostname;
    fd_map[fd.stderr_fd].buffer = &new_node->stderr_buffer;
    fd_map[fd.stderr_fd].type = output_type::STDERR;
    fd_map[fd.stderr_fd].hostname = hostname;

    ssh_nodes[hostname] = new_node;

    return true;
}

bool event_loop::delete_node(std::string hostname) {
    if (ssh_nodes.count(hostname) == 1) {
        ssh_node *discon_node = ssh_nodes.at(hostname);
        delete discon_node;
        ssh_nodes.erase(hostname);
        return true;
    }
    return false;
}

void event_loop::print_prompt() {
    std::cout << "clsh > " << std::flush;
}

void event_loop::input_handler() {
    while (running) {
        std::string input_string;
        print_prompt();
        std::getline(std::cin, input_string);
        if (input_string.length() < 1) {
            input_string = "?";
        }
        std::string command = input_string.substr(1);
        std::string host = "";
        input_type type = INPUT_HOST;
        switch (input_string[0]) {
        case '!':
            type = INPUT_HOST;
            break;
        case '@':
            type = INPUT_SPECIFIC;
            if (command.find(' ') != command.npos) {
                host = command.substr(0, command.find(' '));
                command = command.substr(command.find(' ') + 1);
            }
            break;
        case '%':
            type = INPUT_ALL_CLIENT;
            break;
        case '^':
            type = INPUT_EXIT;
            // running = false;
            break;
        default:
            break;
            type = INPUT_INVALID;
        }
        std::unique_lock<std::mutex> lock(event_queue_lock);
        event_queue.push(event{.type = event_type::INPUT,
                               .input_data = input_event{type, command, host}});
        event_queue_cv.notify_all();
        lock.unlock();
        if (type == INPUT_EXIT) {
            break;
        }
    }
}

void event_loop::output_handler() {
    epoll_event *events = new epoll_event[32];
    sigset_t sigset;
    sigfillset(&sigset);
    while (running) {
        int epoll_event_count = epoll_pwait(epoll_fd, events, 32, 500, &sigset); // blocking
        if (epoll_event_count == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw node_error(std::string("Epoll Failed. ") + strerror(errno));
        }
        for (int i = 0; i < epoll_event_count; i++) {
            fd_info_t &info = fd_map[events[i].data.fd];
            char c = '\0';
            if (events[i].events & EPOLLIN) { // EPOLLIN
                int ret = read(events[i].data.fd, &c, 1);
                if (ret == -1) {
                    throw node_error(std::string("read failed.") + strerror(errno));
                }
                if (info.buffer->append(c)) {
                    std::unique_lock<std::mutex> lock(event_queue_lock);
                    event_queue.push(event{
                        .type = event_type::OUTPUT,
                        .input_data = input_event{input_type::INPUT_INVALID, ""},
                        .output_data = output_event{
                            info.type,
                            info.hostname,
                            info.buffer->to_string()}});
                    event_queue_cv.notify_all();
                    lock.unlock();
                }
            } else if (events[i].events & EPOLLHUP) { // EPOLLRDHUP
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                delete_node(info.hostname);
                std::unique_lock<std::mutex> lock(event_queue_lock);
                event_queue.push(event{
                    .type = event_type::DISCON,
                    .input_data = input_event{input_type::INPUT_INVALID, ""},
                    .output_data = output_event{
                        output_type::OUTPUT_EOF,
                        info.hostname,
                        info.buffer->to_string()}});
                event_queue_cv.notify_all();
                lock.unlock();
            }
        }
    }
}

void event_loop::loop_handler() {
    while (running) {
        std::unique_lock<std::mutex> lock(event_queue_lock);
        if (event_queue.empty()) {
            event_queue_cv.wait(lock);
        }
        event event = event_queue.front();
        event_queue.pop();
        lock.unlock();
        // TODO: print event
        if (event.type == event_type::OUTPUT) {
            std::cout << "\r" + event.output_data.hostname + ": " + event.output_data.data << std::endl;
        } else if (event.type == event_type::INPUT) {
            std::cout << "\rINPUT : " + event.input_data.command + " (" << event.input_data.hostname << ")" << std::endl;
            if (event.input_data.type == INPUT_SPECIFIC) {
                if (ssh_nodes.count(event.input_data.hostname) == 1) {
                    ssh_nodes.at(event.input_data.hostname)->send_input(event.input_data.command);
                } else {
                    std::cout << "\rERROR: " + event.input_data.hostname + " not found" << std::endl;
                }
            } else if (event.input_data.type == INPUT_ALL_CLIENT) {
                for (std::pair<std::string, ssh_node *> node : ssh_nodes) {
                    node.second->send_input(event.input_data.command);
                }
            } else if (event.input_data.type == INPUT_EXIT) {
                running = false;
                std::vector<std::string> keys;
                for (std::pair<std::string, ssh_node *> node : ssh_nodes) {
                    node.second->send_input("exit");
                    keys.push_back(node.first);
                }
                for (std::string &host : keys) {
                    delete_node(host);
                }
            }
        } else if (event.type == event_type::DISCON) {
            std::cout << "\rDISCON : " + event.output_data.hostname << std::endl;
        } else {
            // TODO
            continue;
        }
        if (interactive_mode)
            print_prompt();
    }
}
