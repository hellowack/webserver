#pragma once

const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

//have requests,but can't read data,most likely Request Aborted,
// or data is coming on the way
// for this kind of request, over certain times we discard it
const int AGAIN_MAX_TIMES = 200;

const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0;

const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;


const int METHOD_HEAD = 1;
const int METHOD_GET = 2;
const int METHOD_POST = 3;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

class MimeType
{
private:
	static void init();
	static std::unordered_map<std::string, std::string> mime;
	MimeType();
	MimeType(const MimeType &m);
public:
	static std::string getMime(const std::string &suffix);	
private:
	static pthread_once_t once_control;
};
	
enum HeadersState
{
    h_start = 0,
    h_key,
    h_colon,
    h_spaces_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};


class RequestData : public std::enable_shared_from_this<RequestData>
{
private:
	std::string path;
	int fd;
	int epoll_fd;
	__uint32_t events;

	int method;
	int HTTPVersion;
	std::string file_name;
	int now_read_pos;
	int state;
	bool isFinish;
	bool keep_alive;
	int h_state;
	bool error;
	std::unordered_map<std::string, std::string> headers;

	std::string inBuffer;
	std::string outBuffer;
	
	bool isAbleRead;
	bool isAbleWrite;

	std::weak_ptr<TimerNode> timer;
	
private:
	int parseURI();
	int parseHeaders();
	int analyzeRequest();
public:
	RequestData();
	RequestData(int _epollfd, int _fd, std::string _path);
	~RequestData();
	int getFd();
    void setFd(int _fd);
	void reset();
	void handleConn();
	void handleRead();
	void handleError(int _fd, int err_num, std::string short_msg);
	void handleWrite();
	void enableRead();
    void enableWrite();
    bool canRead();
    bool canWrite();
	void seperateTimer();
	void linkTimer(std::shared_ptr<TimerNode> mtimer);
};
