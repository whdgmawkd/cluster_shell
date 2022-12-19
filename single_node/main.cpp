#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <thread>
#include <vector>

int main() {
    std::cout << SIGRTMIN << " " << SIGRTMAX << std::endl;
    return 0;
}