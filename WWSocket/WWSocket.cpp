#include "WWSocket.h"
void _INIT_WINSOCK() {
	if (!_WINSKINT) {
		WSADATA wdata;
		_WINSKINT = !(WSAStartup(MAKEWORD(2, 2), &wdata));
		if (!_WINSKINT)
			throw exception("fail to initiate WINSOCK");
	}
}
ServerSocket::ServerSocket(SAddr serverAddr) {
	if (!ValidateSAddr(serverAddr)) {
		throw exception("Invalid Address struct");
	}
	this->Addr = serverAddr;
	this->_SockObject = socket(serverAddr.AddrType, SOCK_STREAM, 0);
	if (this->_SockObject == INVALID_SOCKET) {
		throw exception("fail to create socket");//need to close the WSA CLEAN UP
	}
}
bool ServerSocket::Run() {
	if (!_WINSKINT)
		_INIT_WINSOCK();
	if (this->Addr.port == 0 && this->Addr.ipAddr == "") {
		throw exception("No address specified to bind to");
	}
	if (this->_SockObject == INVALID_SOCKET) {
		throw exception("Socket was not created.");
	}
	//binding
	sockaddr_in sockad;
	sockad.sin_port = htons(this->Addr.port);
	sockad.sin_family = this->Addr.AddrType;
	int result = InetPtonA(this->Addr.AddrType, this->Addr.ipAddr.c_str(), &sockad.sin_addr);
	if (!result) {
		throw exception("fail to conver ip address");
	}
	result = bind(this->_SockObject, (sockaddr*) & sockad, sizeof(sockaddr_in));
	if (result == SOCKET_ERROR) {
		throw exception("fail to bind");//use wsagetlasterror
	}
	//multiThreaded Listen and accept phase:
}