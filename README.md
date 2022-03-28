# 一、增加IPV6通信支持

##  1. 客户端支持IPV6

更改`socket()`函数参数

```c++
socket(AF_INET6,...,...)
```

**更改connect()函数**

```c++
#include<ws2ipdef.h>
#include<WS2tcpip.h>
... 
// 2 连接服务器 connect
					sockaddr_in _sin = {};
					_sin.sin_family = AF_INET;
					_sin.sin_port = htons(port);
#ifdef _WIN32
					_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
					_sin.sin_addr.s_addr = inet_addr(ip);
#endif
					//CELLLog_Info("<socket=%d> connecting <%s:%d>...", (int)_pClient->sockfd(), ip, port);
					ret = connect(_pClient->sockfd(), (sockaddr*)&_sin, sizeof(sockaddr_in));
				}
				else {//ipv6
					sockaddr_in6 _sin = {};
					_sin.sin6_scope_id = if_nametoindex(_scope_id_name.c_str());
					_sin.sin6_family = AF_INET6;
					_sin.sin6_port = htons(port);
					inet_pton(AF_INET6, ip, &_sin.sin6_addr);
					ret = connect(_pClient->sockfd(), (sockaddr*)&_sin, sizeof(sockaddr_in6));
				}
```

## 2. 服务端支持IPV6

- 创建套接字部位，与客户端相同`socket(AF_INET6,...,...)`

- 绑定Bind()

```c++ 
if (AF_INET == _address_family)
				{
					sockaddr_in _sin = {};
					_sin.sin_family = AF_INET;
					_sin.sin_port = htons(port);//host to net unsigned short
#ifdef _WIN32
					if (ip) {
						_sin.sin_addr.S_un.S_addr = inet_addr(ip);
					}
					else {
						_sin.sin_addr.S_un.S_addr = INADDR_ANY;
					}
#else
					if (ip) {
						_sin.sin_addr.s_addr = inet_addr(ip);
					}
					else {
						_sin.sin_addr.s_addr = INADDR_ANY;
					}
#endif
					ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
				}
				else if (AF_INET6 == _address_family) {
					sockaddr_in6 _sin = {};
					_sin.sin6_family = AF_INET6;
					_sin.sin6_port = htons(port);
					if (ip)
					{
						inet_pton(AF_INET6, ip, &_sin.sin6_addr);
					}
					else {
						_sin.sin6_addr = in6addr_any;
					}
					ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
				}
				else {
					CELLLog_Error("bind port,_address_family<%d> failed...", _address_family);
					return ret;
				}
```

- 更改接收函数accept()

```c++
			//接受客户端连接
			SOCKET Accept()
			{// 4 accept 等待接受客户端连接
				if (AF_INET == _address_family)
				{
					return Accept_IPv4();
				}
				else {
					return Accept_IPv6();
				}
			}
			SOCKET Accept_IPv6()
			{
				sockaddr_in6 clientAddr = {};
				int nAddrLen = sizeof(clientAddr);
				SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
				cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
				cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
				if (INVALID_SOCKET == cSock)
				{
					CELLLog_PError("accept INVALID_SOCKET...");
				}
				else
				{
					NetWork::make_reuseaddr(cSock);
					//获取IP地址
					static char ip[INET6_ADDRSTRLEN] = {};
					inet_ntop(AF_INET6, &clientAddr.sin6_addr, ip, INET6_ADDRSTRLEN - 1);
					CELLLog_Info("Accept_IP: %s", ip);
					if (_clientAccept < _nMaxClient)
					{
						_clientAccept++;
						//将新客户端分配给客户数量最少的cellServer
						auto c = new Client(cSock, _nSendBuffSize, _nRecvBuffSize);
						c->setIP(ip);
						addClientToCELLServer(c);
					}
					else {
						NetWork::destorySocket(cSock);
						CELLLog_Warring("Accept to nMaxClient");
					}
				}
				return cSock;
			}
			
			SOCKET Accept_IPv4()
			{
				sockaddr_in clientAddr = {};
				int nAddrLen = sizeof(sockaddr_in);
				SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
				cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
				cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
				if (INVALID_SOCKET == cSock)
				{
					CELLLog_PError("accept INVALID_SOCKET...");
				}
				else
				{
					NetWork::make_reuseaddr(cSock);
					//获取IP地址
					char* ip = inet_ntoa(clientAddr.sin_addr);
					CELLLog_Info("Accept_IP: %s", ip);
					if (_clientAccept < _nMaxClient)
					{
						_clientAccept++;
						//将新客户端分配给客户数量最少的cellServer
						auto c = new Client(cSock, _nSendBuffSize, _nRecvBuffSize);
						c->setIP(ip);
						addClientToCELLServer(c);
					}
					else {
						NetWork::destorySocket(cSock);
						CELLLog_Warring("Accept to nMaxClient");
					}
				}
				return cSock;
			}
```

**IOCP注意点：iocp的accept()函数

## 3. IOCP模型下服务端获取客户端IP地址功能

**IPV4地址获取**

```c++
char* getAcceptExAddrs4(IO_DATA_BASE* pIO_DATA)
		{
			int nLocalLen = 0;
			int nRmoteLen = 0;
			sockaddr_in* pLocalAddr_in = NULL;
			sockaddr_in* pRmoteAddr_in = NULL;
			//获取远程和本地的网络地址
			_GetAcceptExAddrs(
				pIO_DATA->wsabuff.buf,
				0,
				sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16,
				(sockaddr**)&pLocalAddr_in,
				&nLocalLen,
				(sockaddr**)&pRmoteAddr_in,
				&nRmoteLen
			);
			//获取IP地址
			//char* ip2 = inet_ntoa(pLocalAddr_in->sin_addr);
			//printf(ip2);
			char* ip = inet_ntoa(pRmoteAddr_in->sin_addr);
			return ip;
		}
```

**IPV6地址获取**

```c++
char* getAcceptExAddrs6(IO_DATA_BASE* pIO_DATA)
			{
				int nLocalLen = 0;
				int nRmoteLen = 0;
				sockaddr_in6* pLocalAddr_in = NULL;
				sockaddr_in6* pRmoteAddr_in = NULL;
				//获取远程和本地的网络地址
				_GetAcceptExAddrs(
					pIO_DATA->wsabuff.buf,
					0,
					sizeof(sockaddr_in6) + 16,
					sizeof(sockaddr_in6) + 16,
					(sockaddr**)&pLocalAddr_in,
					&nLocalLen,
					(sockaddr**)&pRmoteAddr_in,
					&nRmoteLen
				);
				//获取IP地址
				static char ip[INET6_ADDRSTRLEN] = {};
				//inet_ntop(AF_INET6, &pLocalAddr_in->sin6_addr, ip, INET6_ADDRSTRLEN - 1);
				//printf(ip);
				inet_ntop(AF_INET6, &pRmoteAddr_in->sin6_addr, ip, INET6_ADDRSTRLEN - 1);
				return ip;
			}
```



# 二、支持https通信

## 1.判断收到完整HTTP_GET请求

**在之前的收发逻辑中，通过解析报文头，读出数据长度，但由于http通过文本或者字符串传递消息，因此无法再使用之前的消息解析方式**

- http报文中，**请求行与请求头之间插入了一个空白行，即`\r\n`**

- 每一行的结尾也会插入`\r\n`

- 因此，可以利用`\r\n\r\n`来分离请求行与请求头

- 请求行中的`Content-Length`字段说明了 **请求体 **的数据长度，可依据此进一步解析请求体

  - 一般而言，`get`报文中无请求体，`post`报文中才有

- **一个http请求最少应包含20字节**

  - ```
    GET / HTTP/1.1\r\n
    \r\n
    ```

- 因此首先判断报文长度是否大于 20 字节
