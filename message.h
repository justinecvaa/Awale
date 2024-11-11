#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#define BUF_SIZE 1024

struct message {
    uint32_t size;
    char content[BUF_SIZE];
};

#endif // MESSAGE_H
