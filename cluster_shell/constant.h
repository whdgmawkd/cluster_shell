#ifndef __CONSTANT_H__
#define __CONSTANT_H__

enum output_type {
    NO_OUTPUT,
    STDOUT,
    STDERR,
    OUTPUT_EOF
};

enum event_type {
    OUTPUT,
    INPUT,
    DISCON,
    ERROR
};

enum input_type {
    INPUT_HOST,
    INPUT_SPECIFIC,
    INPUT_ALL_CLIENT,
    INPUT_EXIT,
    INPUT_INVALID
};

#endif