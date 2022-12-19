# clsh - Cluster Shell

## Requirements

* ssh
* expect

## Use Scenario

1. Assume that all remote use password authentication
1. each remote usename and password is always ubuntu/ubuntu
    * username and password can be changed via source code
1. 

## Interactive Mode

1. Run command in Host
    * `!command`
1. Run command in Specific Remote
    * `@HOSTNAME command`
1. Run command in All Remote
    * `%command`
1. Stop Specific Remote
    * `@HOSTNAME exit`
1. Quit (Stop all Remote)
    * `^exit`

## Limitation

* ssh password prompt output has no hostname prefix
    * ssh prompt doesn't use stdout. it uses tty device directly. so clsh cannot control them.
* sudo command is not supported

## Build & Run

`mkdir build && cd build && cmake .. && make && ./clsh`