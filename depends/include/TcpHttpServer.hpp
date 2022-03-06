#ifndef _TcpHttpServer_hpp_
#define _TcpHttpServer_hpp_

#include"TcpServerMgr.hpp"
#include"HttpClient.hpp"

namespace doyou {
	namespace io {

		class TcpHttpServer : public TcpServerMgr
		{
		protected:
			virtual void makeClientObj(SOCKET cSock, char* ip)
			{
				auto c = new HttpClient(cSock, _nSendBuffSize, _nRecvBuffSize);
				c->setIP(ip);
				addClientToCELLServer(c);
			}
		};
	}
}
#endif // !_TcpHttpServer_hpp_
