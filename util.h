#pragma once

const int BUFF_MAX_SIZE = 4096;
const int LISTEN_MAX_NUM = 1024;

void handle_sigpipe();
int socket_bind_listen(int port);
int setSocketNonBlocking(int port);
ssize_t readn(int fd, void *buff, size_t n);
ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd, void *buff, size_t n);
ssize_t writen(int fd, std::string &sbuff);
