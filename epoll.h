#pragma once


class Epoll
{
public:
	//typedef std::shared_ptr<RequestData> SP_ReqData;

	static int epoll_init(int event_num, int listen_num);
	static int epoll_add(int fd, std::shared_ptr<RequestData> request, __uint32_t events);
	static int epoll_mod(int fd, std::shared_ptr<RequestData> request, __uint32_t events);
	static int epoll_del(int fd, __uint32_t events = (EPOLLIN | EPOLLET | EPOLLONESHOT));
	static void my_epoll_wait(int listen_fd, int event_num, int timeout);
	static std::vector<std::shared_ptr<RequestData>> getEventsRequest(int listen_fd, int event_num, const std::string path);
	static void acceptConnection(int listen_fd, int epoll_fd, const std::string path);
	static void add_timer(std::shared_ptr<RequestData> request_data, int timeout);
private:
	static const int TIMER_TIME_OUT = 500;
	static const int MAXFD = 1000;
	static epoll_event *events;
	static int epoll_fd;
	static std::shared_ptr<RequestData> fd2req[MAXFD];
	static const std::string PATH;
    static TimerManager timer_manager;	
};
