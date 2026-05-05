#ifndef INPUT_SOCKET_H
#define INPUT_SOCKET_H

#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include "input.h"

void input_socket_init_unix_at_path(char const *path);
void input_socket_init_ip_on_port(char const *port);
void input_socket_init(struct sockaddr *socket_address, socklen_t socket_address_size);

extern struct input_source input_source_socket;

#endif
