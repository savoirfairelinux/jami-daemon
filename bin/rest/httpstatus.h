#pragma once

#include <string>
#include <unordered_map>

static std::unordered_map<int, std::string> statusCodes = {
	// 1xx Informational
	{100, "HTTP/1.1 100 Continue\r\n"},
	{101, "HTTP/1.1 101 Switching Protocols\r\n"},
	{102, "HTTP/1.1 102 Processing\r\n"},

	// 2xx Success
	{200, "HTTP/1.1 200 OK\r\n"},
	{201, "HTTP/1.1 201 Created\r\n"},
	{202, "HTTP/1.1 202 Accepted\r\n"},
	{203, "HTTP/1.1 203 Non-Authoritative Information\r\n"},
	{204, "HTTP/1.1 204 No Content\r\n"},
	{205, "HTTP/1.1 205 Reset Content\r\n"},
	{206, "HTTP/1.1 206 Partial Content\r\n"},
	{207, "HTTP/1.1 207 Multi-Status\r\n"},
	{208, "HTTP/1.1 208 Already Reported\r\n"},
	{226, "HTTP/1.1 226 IM Used\r\n"},

	//3xx Redirection
	{300, "HTTP/1.1 300 Multiple Choices\r\n"},
	{301, "HTTP/1.1 301 Moved Permanently\r\n"},
	{302, "HTTP/1.1 302 Found\r\n"},
	{303, "HTTP/1.1 303 See Other\r\n"},
	{304, "HTTP/1.1 304 Not Modified\r\n"},
	{305, "HTTP/1.1 305 Use Proxy\r\n"},
	{306, "HTTP/1.1 306 Switch Proxy\r\n"},
	{307, "HTTP/1.1 307 Temporary Redirect\r\n"},
	{308, "HTTP/1.1 308 Permanent Redirect\r\n"},

	// 4xx Client Error
	{400, "HTTP/1.1 400 Bad Request\r\n"},
	{401, "HTTP/1.1 401 Unauthorized\r\n"},
	{402, "HTTP/1.1 402 Payment Required\r\n"},
	{403, "HTTP/1.1 403 Forbidden\r\n"},
	{404, "HTTP/1.1 404 Not Found\r\n"},
	{405, "HTTP/1.1 405 Method Not Allowed\r\n"},
	{406, "HTTP/1.1 406 Not Acceptable\r\n"},
	{407, "HTTP/1.1 407 Proxy Authentification Required\r\n"},
	{408, "HTTP/1.1 408 Request Timeout\r\n"},
	{409, "HTTP/1.1 409 Conflict\r\n"},
	{410, "HTTP/1.1 410 Gone\r\n"},
	{411, "HTTP/1.1 411 Length Required\r\n"},
	{412, "HTTP/1.1 412 Precondition Failed\r\n"},
	{413, "HTTP/1.1 413 Payload Too Large\r\n"},
	{414, "HTTP/1.1 414 URI Too Long\r\n"},
	{415, "HTTP/1.1 415 Unsupported Media Type\r\n"},
	{416, "HTTP/1.1 416 Range Not Satisfiable\r\n"},
	{417, "HTTP/1.1 417 Expectation Failed\r\n"},
	{418, "HTTP/1.1 418 I'm a teapot\r\n"},
	{421, "HTTP/1.1 421 Misdirected Request\r\n"},
	{422, "HTTP/1.1 422 Unprocessable Entry\r\n"},
	{423, "HTTP/1.1 423 Locked\r\n"},
	{424, "HTTP/1.1 424 Failed Dependency\r\n"},
	{426, "HTTP/1.1 426 Upgrade Required\r\n"},
	{428, "HTTP/1.1 428 Precondition Required\r\n"},
	{429, "HTTP/1.1 429 Too Many Requests\r\n"},
	{431, "HTTP/1.1 431 Request Header Field Too Large\r\n"},
	{451, "HTTP/1.1 451 Unavailable For Legal Reasons\r\n"},

	// 5xx Server Error
	{500, "HTTP/1.1 500 Internal Server Error\r\n"},
	{501, "HTTP/1.1 501 Not Implemented\r\n"},
	{502, "HTTP/1.1 502 Bad Gateway\r\n"},
	{503, "HTTP/1.1 503 Service Unavailable\r\n"},
	{504, "HTTP/1.1 504 Gateway Timeout\r\n"},
	{505, "HTTP/1.1 505 HTTP Version Not Supported\r\n"},
	{506, "HTTP/1.1 506 Variant Also Negotiates\r\n"},
	{507, "HTTP/1.1 507 Insufficient Storage\r\n"},
	{508, "HTTP/1.1 508 Loop Detected\r\n"},
	{510, "HTTP/1.1 510 Not Extended\r\n"},
	{511, "HTTP/1.1 511 Network Authentication Required\r\n"},
};
