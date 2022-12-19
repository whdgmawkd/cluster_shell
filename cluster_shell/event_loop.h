#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "constant.h"
#include "node.h"

struct input_event {
    input_type type;
    std::string command;
    std::string hostname;
    input_event(input_type t, std::string c, std::string h = "");
};

struct output_event {
    output_type type;
    std::string hostname;
    std::string data;
};

struct clsh_event {
    event_type type;
    std::string hostname;
    node_output output;
};

struct event {
    event_type type;
    input_event input_data;
    output_event output_data;
};

struct fd_info_t {
    output_type type;
    std::string hostname;
    data_buffer *buffer;
};

class event_loop {
  private:
    /**
     * Event Queue
     */
    std::queue<event> event_queue;
    std::mutex event_queue_lock;
    std::condition_variable event_queue_cv;

    /**
     * Remote Node
     */
    std::map<std::string, ssh_node *> ssh_nodes;

    /**
     * epoll
     */
    int epoll_fd;

    // if data_buffer is nullptr, filedescriptor is stdin.
    std::map<int, fd_info_t> fd_map;

    /**
     * Running Flag
     */
    bool running;

    std::thread input_handler_task;
    std::thread output_handler_task;
    std::thread event_loop_task;

    /**
     * Configurations
     */
    bool use_file_stdout;
    std::string file_stdout_dir;
    bool use_file_stderr;
    std::string file_stderr_dir;
    bool interactive_mode;
    std::vector<std::string> commands;

    void input_handler();
    void output_handler();
    void loop_handler();

    void print_prompt();

  public:
    event_loop(bool use_file_stdout = false, std::string file_stdout_dir = "", bool use_file_stderr = false, std::string file_stderr_dir = "", bool interactive_mode = false, std::vector<std::string> commands = {});

    /**
     * Start event loop thread
     */
    void start_event_loop();

    /**
     * Wait until event loop is done
     */
    void wait_event_loop();

    /**
     * add clsh node event
     */
    bool add_event(clsh_event event);

    /**
     * create clsh node
     */
    bool create_node(std::string hostname, std::string username, std::string password);

    /**
     * delete clsh node
     */
    bool delete_node(std::string hostname);

    /**
     * Stop event loop thread
     *
     * function may be blocked while event queue to empty
     */
    void stop_event_loop();
};