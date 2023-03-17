#include <sys/epoll.h>
#include <queue>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <memory>
#include <unordered_map>
#include <functional>
#include <pthread.h>

#include "mutexLock.hpp"
#include "timer.h"
#include "requestData.h"
#include "epoll.h"
#include "util.h"
#include "threadpool.h"

using namespace std;

extern const int LISTEN_MAX_NUM;

const int EVENTS_MAX_NUM = 5000;

const int THREADPOOL_THREAD_NUM = 4;
const int QUEUE_SIZE = 65536;
const int PORT = 8888;


int main()
{
	//set current process as a guard process
	daemon(1, 1);
	handle_sigpipe();
	if(Epoll::epoll_init(EVENTS_MAX_NUM, LISTEN_MAX_NUM) < 0)
	{
		perror("epoll init failed\n");
		return 1;
	}
	if(ThreadPool::threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE) < 0)
	{
		cout<< "Threadpool create failed" <<endl;
		return 1;
	}
	int listen_fd = socket_bind_listen(PORT);
	if(listen_fd < 0)
	{
		perror("socket bind failed\n");
		return 1;
	}
	if(setSocketNonBlocking(listen_fd) < 0)
	{
		perror("set socket non block failed\n");
		return 1;
	}
	shared_ptr<RequestData> request(new RequestData());
	request->setFd(listen_fd);
	if(Epoll::epoll_add(listen_fd, request, EPOLLIN | EPOLLET) < 0)
	{
		perror("epoll add failed\n");
		return 1;
	}

	while(true)
	{
		Epoll::my_epoll_wait(listen_fd, EVENTS_MAX_NUM, -1);
	}
	return 0;
}
