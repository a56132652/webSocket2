#include"Log.hpp"
#include"Config.hpp"
#include"TcpHttpServer.hpp"
#include"sha1.hpp"
#include"base64.hpp"

using namespace doyou::io;

class MyServer:public TcpHttpServer
{
public:
	virtual void OnNetMsgHttp(Server* pServer, HttpClientS* pHttpClient)
	{
		auto strUpgrade = pHttpClient->header_getStr("Upgrade", "");
		if (0 != strcmp(strUpgrade, "websocket"))
		{
			CELLLog_Error("not found Upgrade:websocket");
			return;
		}

		auto cKey = pHttpClient->header_getStr("Sec-WebSocket-Key", nullptr);
		if (!cKey)
		{
			CELLLog_Error("not found Sec-WebSocket-Key");
			return;
		}

		std::string sKey = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		sKey = cKey + sKey;

		unsigned char strSha1[20] = {};
		SHA1_String((const unsigned char*)sKey.c_str(), sKey.length(), strSha1);

		std::string sKeyAccept = Base64Encode(strSha1, 20);


		char resp[256] = {};
		strcat(resp, "HTTP/1.1 101 Switching Protocols\r\n");
		strcat(resp, "Connection: Upgrade\r\n");
		strcat(resp, "Upgrade: websocket\r\n");
		strcat(resp, "Sec-WebSocket-Accept: ");
		strcat(resp, sKeyAccept.c_str());
		strcat(resp, "\r\n\r\n");

		pHttpClient->SendData(resp, strlen(resp));
	}

private:

};

int main(int argc, char* args[])
{
	//设置运行日志名称
	Log::Instance().setLogPath("serverLog", "w", false);
	Config::Instance().Init(argc, args);

	const char* strIP = Config::Instance().getStr("strIP", "any");
	uint16_t nPort = Config::Instance().getInt("nPort", 4567);
	int nThread = Config::Instance().getInt("nThread", 1);

	if (strcmp(strIP, "any") == 0)
	{
		strIP = nullptr;
	}

	MyServer server;
	if (Config::Instance().hasKey("-ipv6"))
	{
		CELLLog_Info("-ipv6");
		server.InitSocket(AF_INET6);
	}
	else {
		CELLLog_Info("-ipv4");
		server.InitSocket();
	}

	server.Bind(strIP, nPort);
	server.Listen(SOMAXCONN);
	server.Start(nThread);

	//在主线程中等待用户输入命令
	while (true)
	{
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			server.Close();
			break;
		}
		else {
			CELLLog_Info("undefine cmd");
		}
	}

	CELLLog_Info("exit.");

	return 0;
}
