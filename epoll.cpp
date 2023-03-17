#include <vector>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unistd.h>
#include <arpa/inet.h>
#include <queue>
#include <functional>

#include "mutexLock.hpp"
#include "timer.h"
#include "util.h"
#include "requestData.h"
#include "threadpool.h"
#include "epoll.h"


using namespace std;

int Epoll::epoll_fd = 0;
epoll_event *Epoll::events;
shared_ptr<RequestData> Epoll::fd2req[MAXFD];
TimerManager Epoll::timer_manager;
const string Epoll::PATH = "/";


int Epoll::epoll_init(int event_num, int listen_num)
{
	epoll_fd = epoll_create(listen_num);
	if(epoll_fd == -1)
		return -1;
	events = new epoll_event[event_num];
	return 0;
}

int Epoll::epoll_add(int fd, shared_ptr<RequestData> request, __uint32_t events)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = events;
	fd2req[fd] = request;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
	{
		perror("epoll_add failed\n");
		return -1;
	}
	return 0;
}

int Epoll::epoll_mod(int fd, shared_ptr<RequestData> request, __uint32_t events)
{
    
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events;
    fd2req[fd] = request;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
    {
        perror("epoll_mod error");
        fd2req[fd].reset();
        return -1;
    }
    return 0;
}

int Epoll::epoll_del(int fd, __uint32_t events)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
        return -1;
    }
    fd2req[fd].reset();
    return 0;
}

void Epoll::my_epoll_wait(int listen_fd, int event_num, int timeout)
{
	int event_count = epoll_wait(epoll_fd, events, event_num, timeout);
	if(event_count < 0)
	{
		perror("epoll_wait error\n");
	}
	vector<shared_ptr<RequestData>> req_data = getEventsRequest(listen_fd, event_count, PATH);
	if(req_data.size() > 0)
	{
		for(auto &req : req_data)
		{
			if(ThreadPool::threadpool_add(req) < 0)
			{
				break;
			}
		}
	}
	timer_manager.handle_expired_events();	
}

//接受新的请求
void Epoll::acceptConnection(int listen_fd, int epoll_fd, const string path)
{
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	socklen_t client_addr_len = sizeof(client_addr);
	int accept_fd = 0;
	
	while((accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
	{
		cout << "client_addr:" << inet_ntoa(client_addr.sin_addr) << std::endl;
		cout << "client port:" << ntohs(client_addr.sin_port) << std::endl;

		if(accept_fd >= MAXFD)
		{
			close(accept_fd);
			continue;
		}

		int ret = setSocketNonBlocking(accept_fd);
		if(ret < 0)
		{
			perror("Set NonBlocking failed\n");
			return;
		}

		shared_ptr<RequestData> req_info(new RequestData(epoll_fd, accept_fd, path));

		__uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
		Epoll::epoll_add(accept_fd, req_info, _epo_event);
		timer_manager.addTimer(req_info, TIMER_TIME_OUT);
	}
}

//分发处理函数
vector<shared_ptr<RequestData>> Epoll::getEventsRequest(int listen_fd, int event_num, const string path)
{
	vector<shared_ptr<RequestData>> req_data;
	for(int i = 0; i < event_num;  ++i)
	{
		int fd = events[i].data.fd;

		if(fd == listen_fd)
		{
			acceptConnection(listen_fd, epoll_fd, path);
		}
		else if(fd < 3)
		{
			cout<<"fd < 3"<<endl;
			break;
		}
		else
		{
			//错误事件删除
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
			{
				cout<<"error event"<<endl;
				if(fd2req[fd])
					fd2req[fd]->seperateTimer();
				fd2req[fd].reset();
				continue;
			}
			shared_ptr<RequestData> cur_req = fd2req[fd];
			if(cur_req)
			{
				//EPOLLPRI：带外数据
				if((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI))
				{
					cur_req->enableRead();
				}
				else
				{
					cur_req->enableWrite();
				}
				//当前结构体监听任务已完成，释放定时器
				cur_req->seperateTimer();
				req_data.emplace_back(cur_req);
				fd2req[fd].reset();
			}
			else
			{
				cout<<"cur_req is invalid"<<endl;
			}
		}
	}
	return req_data;
}

void Epoll::add_timer(shared_ptr<RequestData> request_data, int timeout)
{
	timer_manager.addTimer(request_data, timeout);
}
