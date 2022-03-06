#ifndef _doyou_io_HttpClient_HPP_
#define _doyou_io_HttpClient_HPP_

#include"Client.hpp"
#include<sstream>
#include<map>
#include<vector>

namespace doyou
{
	namespace io
	{
		enum RequestType
		{
			GET = 10,
			POST,
			UNKOWN
		};

		//客户端数据类型
		class HttpClient : public Client
		{
		public:
			HttpClient(SOCKET sockfd = INVALID_SOCKET, int sendSize = SEND_BUFF_SZIE, int recvSize = RECV_BUFF_SZIE)
				:Client(sockfd, sendSize, recvSize)
			{
				netType(netType_http);
			}

			~HttpClient()
			{

			}

			bool hasMsg()
			{
				if(_clientState == clientState_close)
					return false;

				//少于20字符肯定不够http消息
				if (_recvBuff.dataLen() < 20) {
					return false;
				}

				int ret = checkHttpRequest();
				//返回错误or直接关闭连接
				if (ret < 0)
					badRequest();
				return ret > 0;
			}

			// 0 请求消息不完整 继续接收消息
			// -1 请求消息格式错误 返回错误or直接关闭连接
			// -2 不支持的请求消息 返回错误or直接关闭连接
			int checkHttpRequest()
			{
				char* data = _recvBuff.data();
				data[_recvBuff.dataLen()] = 0;
				//查找http消息结束标记
				char* temp = strstr(data, "\r\n\r\n");
				//未找到表示消息还不完整
				if (!temp)
					return 0;
				//偏移到http消息结束处 4=strlen("\r\n\r\n")
				temp += 4;
				//计算http header长度
				_headerLen = temp - data;
				////可能找到的是上一条消息的结束标记
				////毕竟我们没有清空旧数据
				//if (_headerLen > _recvBuff.dataLen())
				//	return 0;
				//判断请求类型是否支持
				temp = data;
				if (temp[0] == 'G' &&
					temp[1] == 'E' &&
					temp[2] == 'T')
				{
					_requestType = RequestType::GET;
				}
				else if (
					temp[0] == 'P' &&
					temp[1] == 'O' &&
					temp[2] == 'S' &&
					temp[3] == 'T')
				{
					_requestType = RequestType::POST;
					//POST还需计算客户端提交的数据长度
					char* p1 = strstr(data, "Content-Length: ");
					//未找到表示消息格式错误
					//返回错误或者直接关闭连接
					if (!p1)
						return -1;
					//加上http消息结束标记 16=strlen("Content-Length: ")
					p1 += 16;
					//"Content-Length: 1024\r\n"
					char* p2 = strchr(p1, '\r');
					if (!p2)
						return -1;
					//数字长度
					int n = p2 - p1;
					//6位数 99万9999 上限是100万字节，就是1MB
					//不过我们只靠缓冲区一次接收。
					//所以POST数据上限是接收缓冲区大小减去_headerLen。
					if (n > 6)
						return -1;
					//6位数 加结束符 用7个char存储
					char lenStr[7] = {};
					strncpy(lenStr, p1, n);
					_bodyLen = atoi(lenStr);
					//数据异常
					if (_bodyLen < 1)
						return -1;
					//POST数据超过了缓冲区可接收的长度
					if (_headerLen + _bodyLen > _recvBuff.buffSize())
						return -1;
					//消息长度>已接收的数据长度，那么数据还没收完
					if (_headerLen + _bodyLen > _recvBuff.dataLen())
						return 0;
				}
				else {
					_requestType = RequestType::UNKOWN;
					return -2;
				}
				return _headerLen;
			}

			//应该在确定消息接收完整时调用此函数
			bool fetch_http_info() {
				std::string request(_recvBuff.data(), _headerLen);
				CELLLog_Info(request.c_str());
				//
				std::istringstream s(request);
				_header_map.clear();

				std::string lineStr;
				std::string::size_type end;
				bool first = true;
				while (std::getline(s, lineStr) && lineStr != "\r")
				{
					//理论上每一行最后一个字符都是\r
					if (lineStr[lineStr.size() - 1] != '\r') {
						return false;
					}
					else {
						//移除\r
						lineStr.erase(lineStr.end() - 1);
					}
					//第一行，请求行
					if (first)
					{
						first = false;
						_header_map["RequestLine"] = lineStr;
						//解析请求行
						request_args(lineStr);
					}
					else {
						//理论上除了第一行都是这个结构"key: value"
						end = lineStr.find(": ", 0);
						if (end != std::string::npos) {
							std::string key = lineStr.substr(0, end);
							std::string value = lineStr.substr(end + 2);//2=strlen(": ");
							_header_map[key] = value;
						}
						else {
							return false;
						}
					}
				}
				//POST 还需要取出POST对应的数据
				if (_requestType == RequestType::POST)
				{
					//我们在calHeaderLen中已经判断过是否有足够的数据
					std::string PostData(_recvBuff.data() + _headerLen, _bodyLen);
					_header_map["RequestBody"] = PostData;
				}

				return true;
			}

			bool request_args(std::string requestLine)
			{
				std::string method;
				std::string url;
				std::string httpVersion;
				
				std::istringstream s1(requestLine);
				std::getline(s1, method, ' ');
				std::getline(s1, url, ' ');
				std::getline(s1, httpVersion, ' ');

				//std::string path;
				std::string args;
				std::istringstream s2(url);
				std::getline(s2, _url_path, '?');
				std::getline(s2, args, '?');

				std::istringstream s3(args);
				std::string val;
				std::string::size_type end;
				_url_args_map.clear();
				while (std::getline(s3, val, '&'))
				{
					//"key=value"
					end = val.find("=", 0);
					if (end != std::string::npos) {
						std::string key = val.substr(0, end);
						std::string value = val.substr(end + 1);//1=strlen("=");
						_url_args_map[key] = value;
					}
				}

				return true;
			}


			void pop_front_msg()
			{
				if (_headerLen > 0 || _bodyLen > 0)
				{
					_recvBuff.pop(_headerLen + _bodyLen);
					_headerLen = 0;
					_bodyLen = 0;
				}
			}

			std::string& getHeaderValue(std::string keyName)
			{
				return _header_map[keyName];
			}

			int WriteString(const std::string &str) {
				std::vector<uint8_t> frame_data;

				return SendData((char*)frame_data.data(), frame_data.size());
			}

			int badRequest(const char * msg = nullptr) {
				_keep_alive = false;
				char response[512] = {};
				strcat(response, "HTTP/1.1 400 Bad Request\r\n");
				strcat(response, "Access-Control-Allow-Origin: *\r\n");
				strcat(response, "Content-Type: text/html;charset=UTF-8\r\n");
				if (msg)
				{
					strcat(response, "Content-Length: ");
					strcat(response, std::to_string(strlen(msg)).c_str());
					strcat(response, "\r\n\r\n");
					strcat(response, msg);
				}else {
					strcat(response, "Content-Length: 25\r\n\r\n");
					strcat(response, "Only support GET or POST.");
				}
				return SendData(response, strlen(response));
			}

			//该函数是根据websocket返回包的格式拼接相应的返回包
			int sendResponse(char *response, const char *retData) {
				std::string ret = retData;
				strcat(response, "HTTP/1.1 200 OK\r\n");
				strcat(response, "Access-Control-Allow-Origin: *\r\n");
				strcat(response, "Content-Type: text/html;charset=UTF-8\r\n");
				strcat(response, "Content-Length: ");
				strcat(response, std::to_string(ret.length()).c_str());
				strcat(response, "\r\n\r\n");
				strcat(response, ret.c_str());
				return SendData(response, strlen(response));
			}

			//该函数是根据websocket返回包的格式拼接相应的返回包
			void sendResponse(std::string& data) {
				std::string ret = "HTTP/1.1 200 OK\r\n";
				ret += "Access-Control-Allow-Origin: *\r\n";
				ret += "Content-Type: text/html;charset=UTF-8\r\n";
				ret += "Content-Length: ";
				ret += std::to_string(data.length());
				ret += "\r\n\r\n";
				ret += data;
				SendData(ret.c_str(), ret.length());
			}

			const char* getStr(const char* argName, const char* def)
			{
				auto itr = _url_args_map.find(argName);
				if (itr == _url_args_map.end())
				{
					CELLLog_Error("request url not find arg <%s>", argName);
				}
				else {
					def = itr->second.c_str();
				}
				return def;
			}

			int getInt(const char* argName, int def)
			{
				auto itr = _url_args_map.find(argName);
				if (itr == _url_args_map.end())
				{
					CELLLog_Error("request url not find arg <%s>", argName);
				}
				else {
					def = atoi(itr->second.c_str());
				}
				return def;
			}

			bool hasKey(const char* key)
			{
				auto itr = _url_args_map.find(key);
				return itr != _url_args_map.end();
			}

			std::string& url_path()
			{
				return _url_path;
			}
		private:
			//请求[http://ke.doyou.tech][/login.php]?[&key=val&pw=123]
			//[域名][路径][参数]

			//[路径]
			std::string _url_path;
			//请求中的[参数]
			std::map<std::string, std::string> _url_args_map;
			//请求头部[key: val]
			std::map<std::string, std::string> _header_map;
			//请求类型 GET POST UNKOWN
			RequestType _requestType = RequestType::UNKOWN;
			//客户端提交的请求长度
			int _headerLen = 0;
			//客户端提交的请求数据长度 
			int _bodyLen = 0;
		};
	}
}
#endif // !_doyou_io_HttpClient_HPP_