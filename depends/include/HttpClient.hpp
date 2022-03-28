#ifndef _doyou_io_HttpClient_HPP_
#define _doyou_io_HttpClient_HPP_

#include"Client.hpp"
#include"SplitString.hpp"

namespace doyou {
	namespace io {
		//客户端数据类型
		class HttpClient :public Client
		{
		public:
			enum RequestType
			{
				GET = 10,
				POST,
				UNKOWN
			};
		public:
			HttpClient(SOCKET sockfd = INVALID_SOCKET, int sendSize = SEND_BUFF_SZIE, int recvSize = RECV_BUFF_SZIE) :
				Client(sockfd, sendSize, recvSize)
			{

			}

			virtual bool hasMsg()
			{
				//完整的http请求一定超过20字节
				if (_recvBuff.dataLen() < 20)
					return false;

				int ret = checkHttpRequest();
				if (ret < 0)
					badRequest();
				return ret > 0;
			}
			// 0 请求的消息不完整 继续等待消息
			// -1 不支持的请求类型
			int checkHttpRequest()
			{
				//查找http请求消息结束标记
				char* temp = strstr(_recvBuff.data(), "\r\n\r\n");
				//未找到表示消息还不完整
				if (!temp)
					return 0;
				CELLLog_Info(_recvBuff.data());
				//偏移到消息结束位置
				//4=strlen("\r\n\r\n")
				temp += 4;
				//计算http请求消息的请求行+请求头长度
				_headerLen = temp - _recvBuff.data();
				//判断请求类型是否支持
				temp = _recvBuff.data();
				if (temp[0] == 'G' &&
					temp[1] == 'E' &&
					temp[2] == 'T')
				{
					_requestType = HttpClient::GET;
				}
				else if (
					temp[0] == 'P' &&
					temp[1] == 'O' &&
					temp[2] == 'S' &&
					temp[3] == 'T')
				{
					_requestType = HttpClient::POST;
				}
				else {
					_requestType = HttpClient::UNKOWN;
					return -1;
				}

				return _headerLen;
			}


			//解析http请求
			//确定收到完整http请求消息的时候才能调用
			bool getRequestInfo()
			{
				//判断是否已经收到了完整请求
				if (_headerLen <= 0)
					return false;
				//清除上一个消息请求的数据
				_header_map.clear();

				SplitString ss;
				ss.set(_recvBuff.data());
				//请求行示例："GET /login.php?a=5 HTTP/1.1\r\n"
				char* temp = ss.get("\r\n");
				if (temp)
				{
					_header_map["RequestLine"] = temp;
					request_args(temp);
				}

				//请求头示例："Connection: Keep-Alive\r\n"
				while (true)
				{
					//请求头每一行都有一个\r\n
					temp = ss.get("\r\n");
					if (temp)
					{
						//"Connection: Keep-Alive\0\n"
						SplitString ss2;
						ss2.set(temp);
						//每个请求头字段都是"key: val\r\n"
						char* key = ss2.get(": ");
						char* val = ss2.get(": ");
						if (key && val)
						{
							//key = Connection
							//val = Keep-Alive
							_header_map[key] = val;
						}
					}
					else {
						break;
					}
				}
				return true;
			}

			bool request_args(char* requestLine)
			{
				//requestLine="GET /login.php?a=5 HTTP/1.1"
				SplitString ss;
				ss.set(requestLine);
				//requestLine="GET\0/login.php?a=5 HTTP/1.1"
				_method = ss.get(' ');
				if (!_method)
					return false;

				_url = ss.get(' ');
				if (!_url)
					return false;

				_httpVersion = ss.get(' ');
				if (!_httpVersion)
					return false;

				ss.set(_url);
				_url_path = ss.get('?');
				if (!_url_path)
					return false;

				_url_args = ss.get('?');
				if (!_url_args)
					return true;

				while (true)
				{
					char* temp = ss.get('&');
					if (temp)
					{
						SplitString ss2;
						ss2.set(temp);
						char* key = ss2.get('=');
						char* val = ss2.get('=');
						if (key && val)
							_args_map[key] = val;
					}
					else {
						break;
					}
				}

				return true;
			}

			virtual void pop_front_msg()
			{
				if (_headerLen > 0)
				{
					_recvBuff.pop(_headerLen);
					_headerLen = 0;
				}
			}

			void badRequest()
			{
				std::string response = "HTTP/1.1 400 Bad Request\r\n";
				response += "Content-Type: text/html;charset=UTF-8\r\n";
				response += "Content-Length: 25\r\n";
				response += "\r\n";
				response += "Only support GET or POST.";
				SendData(response.c_str(), response.length());
			}

		protected:
			int _headerLen = 0;
			std::map<std::string, char*> _header_map;
			std::map<std::string, char*> _args_map;
			RequestType _requestType = HttpClient::UNKOWN;
			char* _method;
			char* _url;
			char* _url_path;
			char* _url_args;
			char* _httpVersion;
		};
	}
}
#endif // !_doyou_io_HttpClient_HPP_