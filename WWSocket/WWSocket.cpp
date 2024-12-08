#include "WWSocket.h"
enum class Level {
	Fault,
	Warning,
	Info
};
void logger(const char* data,Level lv) {
	tm* now = localtime(nullptr);
	switch (lv) {
		case Level::Fault:
			cout << "[Fault] " << put_time(now, "%H:%M:%S") << ": " << data;
			break;
		case Level::Warning:
			cout << "[Warning] " << put_time(now, "%H:%M:%S") << ": " << data;
			break;
		case Level::Info:
			cout << "[INF] " << put_time(now, "%H:%M:%S") << ": " << data;
			break;
	}
}
bool ConvertSocketAddrIPPORT(const sockaddr_in* sockad, char* ip, int port) {
	ip = inet_ntoa(sockad->sin_addr);
	port = ntohs(sockad->sin_port);
	if (ip == NULL)
		return false;
	return true;
}
void _INIT_WINSOCK() {
	if (!_WINSKINT) {
		WSADATA wdata;
		_WINSKINT = !(WSAStartup(MAKEWORD(2, 2), &wdata));
		if (!_WINSKINT) {
			throw exception("fail to initiate WINSOCK");
		}

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
void Server_ClientListener(SClientDesc clientDsc,ServerSocket sk) {
	int rc = 0;
	do{
		char *buf = new char[BUFFERSIZE];
		rc = recv(clientDsc.clientSocket, buf, BUFFERSIZE, 0);
		if (rc == SOCKET_ERROR) {
			logger((string("fail in rcv for") + string(clientDsc.clientAddr.ipAddr) + string(" : ") + string(to_string(WSAGetLastError()))).c_str(), Level::Fault);
		}
		else if (!rc) {
			if (sk.OnClientEvent) {
				sk.OnClientEvent(clientDsc, ClientStatus::Disconnected);
			}
			clientDsc.status = ClientStatus::Disconnected;
			sk.Clients.erase(remove(sk.Clients.begin(), sk.Clients.end(), clientDsc));
		}
		if (rc > 0) {
			SData data;
			data.data = buf;
			data.size = rc;
			if (sk.OnReplayEvent)
				sk.OnReplayEvent(clientDsc, &data);
		}
	}
	while (clientDsc.status == ClientStatus::Connected && !rc && sk.IsRunning);
	
}
bool ServerSocket::Run() {
	if (this->IsRunning)
		return true;
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
	if (!(this->OnClientEvent) || !(this->OnReplayEvent)) {
		throw exception("no ClientEvent listener passed");
	}
	result = listen(this->_SockObject, 20);
	if (result == SOCKET_ERROR) {
		throw exception("fail to put socket in listen mode");
	}
	SOCKET _sk = this->_SockObject;
	thread ListenThread = thread([this,_sk]() {
		while (this->IsRunning &&this->Clients.size() <= MAXCLIENT) {
			sockaddr clientAddr;
			int clientAddr_len = 0;
			SOCKET clientSocket = accept(_sk, &clientAddr, &clientAddr_len);
			if (clientSocket != INVALID_SOCKET) {
				SAddr clientAddr = { 0 };
				int port = 0;
				char* ipString = new char[15];
				if (!ConvertSocketAddrIPPORT((sockaddr_in*)&clientAddr, ipString, port)) {
					logger("fail to convert ip and port for client", Level::Warning);
					clientAddr.ipAddr = "NOADDR";
					clientAddr.port = 0;
				}
				clientAddr.ipAddr = ipString;
				clientAddr.port = port;
				SClientDesc clientDSC = {};
				clientDSC.clientSocket = clientSocket;
				clientDSC.clientAddr = clientAddr;
				clientDSC.status = ClientStatus::Connected;
				this->Clients.push_back(clientDSC);
				thread clistenThread = thread(Server_ClientListener, clientDSC, this);
				clientDSC.ClientListenerThread = &clistenThread;
			}
		}
	});
}
bool ServerSocket::Shutdown() {
	this->IsRunning = false;
	return this->IsRunning;
}
SAddr ServerSocket::GetSAddr() {
	return this->Addr;
}
bool ServerSocket::SendTo(SClientDesc& clientDsc, SData* data) {
	if (clientDsc.status == ClientStatus::Disconnected) {
		logger((string("fail to send to socket: ") + clientDsc.clientAddr.ipAddr).c_str(), Level::Warning);
		return false;
	}
	int r = send(clientDsc.clientSocket, data->data, data->size, 0);
	if (r == SOCKET_ERROR) {
		logger((string("fail to send to socket: ") + clientDsc.clientAddr.ipAddr + string(" ") + to_string(WSAGetLastError())).c_str(), Level::Warning);
		return false;
	}
	return true;
}
void ServerSocket::SendToAll(SData* data) {
	if (this->Clients.size() > 0) {
		for (int i{ 0 }; i < this->Clients.size(); i++) {
			SClientDesc& client = this->Clients[i];
			SOCKET sk = client.clientSocket;
			int r = send(sk, data->data, data->size, 0);
			if (r == SOCKET_ERROR) {
				logger((string("fail to mass send to socket: ") + client.clientAddr.ipAddr + string(" ") + to_string(WSAGetLastError())).c_str(), Level::Warning);
			}

		}
	}
}

//CLIENT FUNCTION DEFINITIONS: