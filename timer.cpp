#include <memory>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <sys/time.h>
#include <string>
#include <deque>
#include <queue>
#include <sys/epoll.h>

#include "mutexLock.hpp"
#include "timer.h"
#include "requestData.h"
#include "epoll.h"
#include "timer.h"


using namespace std;

TimerNode::TimerNode(shared_ptr<RequestData> _request_data, int timeout):
	deleted(false),
	request_data(_request_data)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

TimerNode::~TimerNode()
{
	if(request_data)
	{
		Epoll::epoll_del(request_data->getFd());
	}
}
void TimerNode::update(int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) +timeout;
}
bool TimerNode::isValid()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	size_t tmp = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
	if(tmp < expired_time)
	{
		return true;
	}
	else
	{
		this->setDeleted();
		return false;
	}
}
void TimerNode::clearReq()
{
	request_data.reset();
	this->setDeleted();
}
void TimerNode::setDeleted()
{
	deleted = true;
}
bool TimerNode::isDeleted() const
{
	return deleted;
}
size_t TimerNode::getExpireTime() const
{
	return expired_time;
}

void TimerManager::addTimer(shared_ptr<RequestData> request_data, int timeout)
{
	shared_ptr<TimerNode> new_node(new TimerNode(request_data, timeout));
	{
		MutexLockGuard locker(this->lock);
		timerNodeQueue.push(new_node);
	}
	request_data->linkTimer(new_node);
}	

void TimerManager::handle_expired_events()
{
	MutexLockGuard locker(lock);
	while(!timerNodeQueue.empty())
	{
		shared_ptr<TimerNode> ptimer_now = timerNodeQueue.top();
		if( (ptimer_now->isDeleted()) || (ptimer_now->isValid() == false))
		{
			timerNodeQueue.pop();
		}
		else
		{
			break;
		}
	}
}
