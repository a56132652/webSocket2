#ifndef _doyou_io_WebSocketClientS_HPP_
#define _doyou_io_WebSocketClientS_HPP_

#include"HttpClientS.hpp"
#include"sha1.hpp"
#include"base64.hpp"

namespace doyou {
	namespace io {

		enum WebSocketOpcode {
			opcode_CONTINUATION = 0x0,
			opcode_TEXT = 0x1,
			opcode_BINARY = 0x2,
			opcode_CLOSE = 0x8,
			opcode_PING = 0x9,
			opcode_PONG = 0xA,
		};

		//WebSocket数据帧头部协议格式
		struct WebSocketHeader {
			//Extended payload length 16或64位
			uint64_t len;
			//操作代码，Opcode 的值决定了应该如何解析后续的 数据载荷（data payload）
			//如果操作代码是不认识的，那么接收端应该 断开连接（fail the connection）
			WebSocketOpcode opcode;
			//所有从客户端传送到服务端的数据帧，数据载荷都进行了掩码操作，
			//Mask 为 1，且携带了 4 字节的 Masking-key。
			//如果 Mask 为 0，则没有 Masking-key
			//备注：载荷数据的长度len，不包括 mask key 的长度。
			uint8_t masking_key[4];
			//如果值为true，表示这是 消息（message）的最后一个分片（fragment）
			//如果是 0，表示不是 消息（message）的最后一个 分片（fragment）
			bool fin;
			//表示是否要对数据（消息体）进行掩码操作
			//从客户端向服务端发送数据时，需要对数据进行掩码操作；
			//从服务端向客户端发送数据时，不需要对数据进行掩码操作。
			//如果服务端接收到的数据没有进行过掩码操作，服务端需要断开连接。
			bool mask;
			//数据载荷的长度,如果值为x
			//x 为 0~126：数据的长度为 x 字节。
			//x 为 126：后续 2 个字节代表一个 16 位的无符号整数，该无符号整数的值为数据的长度。
			//x 为 127：后续 8 个字节代表一个 64 位的无符号整数（最高位为 0），该无符号整数的值为数据的长度。
			//此外，如果 payload length 占用了多个字节的话，payload length 的二进制表达采用 网络序（big endian，重要的位在前）。
			uint8_t len0;
			//WebSocket数据帧头部字节长度 
			uint8_t header_size;
		};
		//客户端数据类型
		class WebSocketClientS :public HttpClientS
		{
		public:
			WebSocketClientS(SOCKET sockfd = INVALID_SOCKET, int sendSize = SEND_BUFF_SZIE, int recvSize = RECV_BUFF_SZIE) :
				HttpClientS(sockfd, sendSize, recvSize)
			{

			}

			bool handshake()
			{
				auto strUpgrade = this->header_getStr("Upgrade", "");
				if (0 != strcmp(strUpgrade, "websocket"))
				{
					CELLLog_Error("WebSocketClientS::handshake, not found Upgrade:websocket");
					return false;
				}

				auto cKey = this->header_getStr("Sec-WebSocket-Key", nullptr);
				if (!cKey)
				{
					CELLLog_Error("WebSocketClientS::handshake, not found Sec-WebSocket-Key");
					return false;
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

				this->SendData(resp, strlen(resp));

				return true;
			}
			virtual bool hasMsg()
			{
				if (clientState_join == _clientState)
				{
					return HttpClientS::hasMsg();
				}
				else if (clientState_run == _clientState)
				{
					return hasMsgWS();
				}
			}

			virtual void pop_front_msg()
			{
				HttpClientS::pop_front_msg();
				if (_wsh.header_size > 0)
				{
					_recvBuff.pop(_wsh.header_size + _wsh.len);
					_wsh.header_size = 0;
					_wsh.len = 0;
				}
				//if (clientState_join == _clientState)
				//{
				//	
				//}
				//else if (clientState_run == _clientState)
				//{
				//}
			}

			virtual bool hasMsgWS()
			{
				//完整的WebSocket数据帧头部数据一定不小于2字节
				if (_recvBuff.dataLen() < 2)
					return false;
				//将数据转化为无符号整数数据
				const uint8_t* data = (const uint8_t*)_recvBuff.data();
				//
				_wsh.header_size = 2;
				_wsh.fin = ((data[0] & 0x80) == 0x80);
				//
				_wsh.opcode = static_cast<WebSocketOpcode>(data[0] & 0xF);
				if (opcode_CLOSE == _wsh.opcode)
				{
					state(clientState_close);
					return true;
				}
				//
				_wsh.mask = ((data[1] & 0x80) == 0x80);
				if (_wsh.mask)
				{
					_wsh.header_size += 4;
				}
				else {
					state(clientState_close);
					return true;
				}
				//
				_wsh.len0 = (data[1] & 0x7F);

				if (_wsh.len0 == 126)
				{
					_wsh.header_size += 2;
				}
				else if (_wsh.len0 == 127)
				{
					_wsh.header_size += 8;
				}

				//数据帧头部数据一定不小于header_size字节
				if (_recvBuff.dataLen() < _wsh.header_size)
					return false;

				//
				if (_wsh.len0 == 126)
				{
					//_wsh.len = 0
					//0000 0000 0000 0000

					//0000 1111 0000 0000
					_wsh.len |= data[2] << 8;
					//0000 1111 0000 0000

					//0000 0000 0000 1111
					_wsh.len |= data[3] << 0;
					//0000 1111 0000 1111
				}
				else if (_wsh.len0 == 127)
				{
					_wsh.len |= data[2] << 56;
					_wsh.len |= data[3] << 48;
					_wsh.len |= data[4] << 40;
					_wsh.len |= data[5] << 32;
					_wsh.len |= data[6] << 24;
					_wsh.len |= data[7] << 16;
					_wsh.len |= data[8] << 8;
					_wsh.len |= data[9] << 0;
				}
				else {
					_wsh.len = _wsh.len0;
				}

				//判断数据收完没有
				if (_recvBuff.dataLen() < _wsh.header_size + _wsh.len)
					return false;

				return true;
			}

			char* fetch_data()
			{
				char* rbuf = _recvBuff.data() + _wsh.header_size;

				//将数据转化为无符号整数数据
				if (_wsh.mask)
				{
					const uint8_t* data = (const uint8_t*)_recvBuff.data();
					_wsh.masking_key[0] = data[_wsh.header_size - 4];
					_wsh.masking_key[1] = data[_wsh.header_size - 3];
					_wsh.masking_key[2] = data[_wsh.header_size - 2];
					_wsh.masking_key[3] = data[_wsh.header_size - 1];

					for (uint64_t i = 0; i < _wsh.len; ++i)
					{
						rbuf[i] ^= _wsh.masking_key[i & 0x3];
					}
				}

				return rbuf;
			}
		private:
			WebSocketHeader _wsh = {};
		};
	}
		};
	}

}
#endif // !_doyou_io_WebSocketClientS_HPP_
