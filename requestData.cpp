#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <queue>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <pthread.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include "mutexLock.hpp"
#include "timer.h"
#include "epoll.h"
#include "util.h"
#include "requestData.h"

using namespace std;
using namespace cv;
extern const int BUFF_MAX_SIZE;

pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
unordered_map<std::string, std::string> MimeType::mime;

void MimeType::init()
{
    mime[".html"] = "text/html";
    mime[".avi"] = "video/x-msvideo";
    mime[".bmp"] = "image/bmp";
    mime[".c"] = "text/plain";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html";
    mime[".ico"] = "application/x-ico";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain";
    mime[".mp3"] = "audio/mp3";
	mime[".css"] = "text/css";
    mime["default"] = "text/html";
}

string MimeType::getMime(const string &suffix)
{
	pthread_once(&once_control, MimeType::init);
	if(mime.find(suffix) == mime.end())
		return mime["default"];
	else 
		return mime[suffix];
}

RequestData::RequestData():
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_start),
	keep_alive(false),
	isAbleRead(true),
	isAbleWrite(false),
	events(0),
	error(false)
{
	cout << "RequestData()" << endl;
}
RequestData::RequestData(int _epollfd, int _fd, string _path):
	now_read_pos(0),
	state(STATE_PARSE_URI),
	h_state(h_start),
	keep_alive(false),
	path(_path),
	fd(_fd),
	epoll_fd(_epollfd),
	isAbleRead(true),
	isAbleWrite(false),
	events(0),
	error(false)
{
	cout<< "RequestData()" << endl;
}
RequestData::~RequestData()
{
	cout << "~RequestData()" << endl;
	close(fd);
}

int RequestData::getFd()
{
    return fd;
}
void RequestData::setFd(int _fd)
{
    fd = _fd;
}

void RequestData::reset()
{
    inBuffer.clear();
    file_name.clear();
    path.clear();
    now_read_pos = 0;
    state = STATE_PARSE_URI;
    h_state = h_start;
    headers.clear();
    //keep_alive = false;
    if (timer.lock())
    {
        shared_ptr<TimerNode> my_timer(timer.lock());
        my_timer->clearReq();
        timer.reset();
    }
}

void RequestData::enableRead()
{
    isAbleRead = true;
}
void RequestData::enableWrite()
{
    isAbleWrite = true;
}
bool RequestData::canRead()
{
    return isAbleRead;
}
bool RequestData::canWrite()
{
    return isAbleWrite;
}

/*
	GET message format:
	
	GET http://www.baidu.com/index.html HTTP/1.0
	User-Agent: Wget/1.12(linux-gnu)
	Host: www.baidu.com
	Connection: close
*/

int RequestData::parseURI()
{
	string &str = inBuffer;
	//begin parse request until read the complete request line
	int pos = str.find('\n', now_read_pos);
	if(pos < 0)
	{
		return PARSE_URI_AGAIN;
	}
	//remove the space of the request line to save memory
	string request_line = str.substr(0, pos);
	if(str.size() > pos + 1)
		str = str.substr(pos + 1);
	else
		str.clear();
	
	
	// Method
	int posGet = request_line.find("GET");
	int posPost = request_line.find("POST");
	int posHead = request_line.find("HEAD");

	if (posGet >= 0) {
		pos = posGet;
		method = METHOD_GET;
	} 
	else if (posPost >= 0) {
		pos = posPost;
		method = METHOD_POST;
	} else if (posHead >= 0) {
		pos = posHead;
		method = METHOD_HEAD;
	} else {
		return PARSE_URI_ERROR;
	}


	//GET http://www.baidu.com/index.html HTTP/1.0
	//file name
	pos = request_line.find("/", pos);
	if(pos < 0)
		return PARSE_URI_ERROR;
	else
	{
		int _pos = request_line.find(' ', pos);
		if(_pos < 0)
			return PARSE_URI_ERROR;
		else
		{
			if(_pos - pos > 1)
			{
				file_name = request_line.substr(pos + 1, _pos - pos - 1);
				int __pos = file_name.find('?');
				if(__pos >= 0)
				{
					file_name = file_name.substr(0, __pos);
				}
			}
			else
				file_name = "index.html";
		}
		pos = _pos;
	}
	//HTTP version
	pos = request_line.find("/", pos);
	if(pos < 0)
		return PARSE_URI_ERROR;
	else
	{
		if(request_line.size() - pos <= 3)
			return PARSE_URI_ERROR;
		else
		{
			string version = request_line.substr(pos + 1, 3);
			if(version == "1.0")
				HTTPVersion = HTTP_10;
			else if(version== "1.1")
				HTTPVersion = HTTP_11;
			else
				return PARSE_URI_ERROR;
		}
	}
	return PARSE_URI_SUCCESS;
}

int RequestData::parseHeaders()
{
	string &str = inBuffer;
	int key_start = -1, key_end = -1, value_start = -1, value_end = -1;
	int now_read_line_begin = 0;
	bool notFinish = true;
	for(int i = 0; i < str.size() && notFinish; ++i)
	{
		switch(h_state)
		{
			case h_start:
			{
				if(str[i] == '\n' || str[i] == '\r')
					break;
				h_state = h_key;
				key_start = i;
				now_read_line_begin = i;
				break;
			}
			case h_key:
			{
				if(str[i] == ':')
				{
					key_end = i;
					if(key_end - key_start <= 0)
						return PARSE_HEADER_ERROR;
					h_state = h_colon;
				}
				else if (str[i] == '\n' || str[i] == '\r')
                    return PARSE_HEADER_ERROR;
                break;  
            }
            case h_colon:
            {
                if (str[i] == ' ')
                {
                    h_state = h_spaces_after_colon;
                }
                else
                    return PARSE_HEADER_ERROR;
                break;  
            }
            case h_spaces_after_colon:
            {
                h_state = h_value;
                value_start = i;
                break;  
            }
            case h_value:
            {
                if (str[i] == '\r')
                {
                    h_state = h_CR;
                    value_end = i;
                    if (value_end - value_start <= 0)
                        return PARSE_HEADER_ERROR;
                }
                else if (i - value_start > 255)
                    return PARSE_HEADER_ERROR;
                break;  
            }
            case h_CR:
            {
                if (str[i] == '\n')
                {
                    h_state = h_LF;
                    string key(str.begin() + key_start, str.begin() + key_end);
                    string value(str.begin() + value_start, str.begin() + value_end);
                    headers[key] = value;
                    now_read_line_begin = i;
                }
                else
                    return PARSE_HEADER_ERROR;
                break;  
            }
            case h_LF:
            {
                if (str[i] == '\r')
                {
                    h_state = h_end_CR;
                }
                else
                {
                    key_start = i;
                    h_state = h_key;
                }
                break;
            }
            case h_end_CR:
            {
                if (str[i] == '\n')
                {
                    h_state = h_end_LF;
                }
                else
                    return PARSE_HEADER_ERROR;
                break;
            }
            case h_end_LF:
            {
                notFinish = false;
                key_start = i;
                now_read_line_begin = i;
                break;
            }
        }
    }
    if (h_state == h_end_LF)
    {
        str = str.substr(now_read_line_begin);
        return PARSE_HEADER_SUCCESS;
    }
    str = str.substr(now_read_line_begin);
    return PARSE_HEADER_AGAIN;
}

int RequestData::analyzeRequest()
{
	if(method == METHOD_POST)
	{
		/*string header;
		header += string("HTTP/1.1 200 OK\r\n");
		if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
		{
			keep_alive = true;
			header += string("Connection: keep-alive\r\n") + "Keep-Alive: timeout=" + to_string(5 * 60 * 1000) + "\r\n";
		}*/
		return ANALYSIS_ERROR;
	}
	else if(method == METHOD_GET|| method == METHOD_HEAD)
	{
		string header;
		header += "HTTP/1.1 200 OK\r\n";
		if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
		{
			keep_alive = true;
			header += string("Connection: keep-alive\r\n") + "Keep-Alive: timeout =" + to_string(5 *60 * 1000) + "\r\n";
		}
		int dot_pos = file_name.find('.');
		string filetype;
		if(dot_pos < 0)
			filetype = MimeType::getMime("default");
		else
			filetype = MimeType::getMime(file_name.substr(dot_pos));
		//cout << "filetype:" << filetype << endl;
		struct stat sbuf;
		if(stat(file_name.c_str(), &sbuf) < 0)
		{
			header.clear();
			handleError(fd, 404, "Not Found!");
			return ANALYSIS_ERROR;
		}
		header += "Content-type: " + filetype + "\r\n";
		header += "Content-length: " + to_string(sbuf.st_size) + "\r\n";
	    //header is over
		header += "\r\n";
		outBuffer += header;

		if(method == METHOD_HEAD)
			return ANALYSIS_SUCCESS;
		
		if(filetype == "image/jpeg")
		{
			Mat src = imread(file_name);
			vector<unsigned char> data_encode;
       			imencode(".jpg", src, data_encode);
			outBuffer += string(data_encode.begin(), data_encode.end());
			return ANALYSIS_SUCCESS;
		}
		int src_fd = open(file_name.c_str(), O_RDONLY, 0);
		char *src_addr = static_cast<char*>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
		close(src_fd);
		outBuffer += src_addr;
		munmap(src_addr, sbuf.st_size);
		return ANALYSIS_SUCCESS;	
	}	
	else
		return ANALYSIS_ERROR;
}

void RequestData::handleError(int fd, int err_num, string short_msg)
{
    short_msg = " " + short_msg;
    char send_buff[BUFF_MAX_SIZE];
    string body_buff, header_buff;
    body_buff += "<html><title>oops!something goes wrong!</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += to_string(err_num) + short_msg;
    body_buff += "<hr><em> hellowack's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-type: text/html\r\n";
    header_buff += "Connection: close\r\n";
    header_buff += "Content-length: " + to_string(body_buff.size()) + "\r\n";
    header_buff += "\r\n";

    sprintf(send_buff, "%s", header_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
}

void RequestData::handleRead()
{
	do
	{
		int read_num = readn(fd, inBuffer);
		if(read_num < 0)
		{
			perror("read failed\n");
			error = true;
			handleError(fd, 400, "Bad Request");
			break;
		}
		else if(read_num == 0)
		{
			//reason:maybe is request aborted, maybe the data not come yet
			//most likely, the client is closed,we handle this situation all as the client is closed
			error = true;
			break;
		}

		if(state == STATE_PARSE_URI)
		{
			int flag = this->parseURI();
			if(flag == PARSE_URI_AGAIN)
				break;
			else if(flag == PARSE_URI_ERROR)
			{
				perror("2\n");
				error = true;
				handleError(fd, 400, "Bad Request!");
				break;
			}	
			else 
				state = STATE_PARSE_HEADERS;		
		}
		if(state == STATE_PARSE_HEADERS)
		{
			int flag = this->parseHeaders();
			if(flag == PARSE_HEADER_AGAIN)
				break;
			else if(flag == PARSE_HEADER_ERROR)
			{
				perror("3\n");
				error = true;
				handleError(fd, 400, "Bad Request");
				break;
			}
			if(method == METHOD_POST)
			{
				//prepare to receive the body content 
				state = STATE_RECV_BODY;
			}
			else
			{
				state = STATE_ANALYSIS;
			}
		}
		if(state == STATE_RECV_BODY)
		{
			int content_length = 0;
			if(headers.find("Content-length") != headers.end())
			{
				content_length = stoi(headers["Content-length"]); 
			}
			else
			{
				error = true;
				handleError(fd, 400, "Bad Request: lack of argument:Content-Length");
				break;
			}
			if(inBuffer.size() < content_length)
				break;
			state = STATE_ANALYSIS;
		}
		if(state == STATE_ANALYSIS)
		{
			int flag = this->analyzeRequest();
			if(flag == ANALYSIS_SUCCESS)
			{
				state = STATE_FINISH;
				break;
			}
			else
			{
				error = true;
				break;
			}
		}
	}while(false);

	if(!error)
	{
		if(outBuffer.size() > 0)
			events |= EPOLLOUT;
		if(state == STATE_FINISH)
		{
			cout << "keep-alive = " << keep_alive << endl;
			if(keep_alive)
			{
				this->reset();
				events |= EPOLLIN;
			}
			else
				return;
		}
		else
			events |= EPOLLIN;
	}
}

void RequestData::handleWrite()
{
	if(!error)
	{
		if(writen(fd, outBuffer) < 0)
		{
			perror("writen failed\n");
			events = 0;
			error = true;
		}
		else if(outBuffer.size() > 0)
			events |= EPOLLOUT;
	}
}

void RequestData::handleConn()
{
	if(!error)
	{
		if(events != 0)
		{
			//add time info
			int timeout = 2000;
			if(keep_alive)
				timeout = 5 * 60 * 1000;
			isAbleRead = false;
			isAbleWrite = false;
			Epoll::add_timer(shared_from_this(), timeout);
			if((events & EPOLLIN) && (events & EPOLLOUT))
			{
				events = __uint32_t(0);
				events |= EPOLLOUT;
			}
			events |= (EPOLLET | EPOLLONESHOT);
			__uint32_t _events = events;
			events = 0;
			if(Epoll::epoll_mod(fd, shared_from_this(), _events) < 0)
			{
				cout << "Epoll::epoll_mod error" << endl;
			}
		}
		else if(keep_alive)
		{
			events |= (EPOLLIN | EPOLLET | EPOLLONESHOT);
			int timeout = 5 * 60 * 1000;
			isAbleRead = false;
			isAbleWrite = false;
			Epoll::add_timer(shared_from_this(), timeout);
			__uint32_t _events = events;
			events = 0;
			if(Epoll::epoll_mod(fd, shared_from_this(), _events) < 0)
			{
				cout << "Epoll::epoll_mod error" << endl;
			}
		}
	}
}

void RequestData::seperateTimer()
{
	if(timer.lock())
	{
		shared_ptr<TimerNode> my_timer(timer.lock());
		my_timer->clearReq();
		timer.reset();
	}
}

void RequestData::linkTimer(shared_ptr<TimerNode> mtimer)
{
    timer = mtimer;
}

