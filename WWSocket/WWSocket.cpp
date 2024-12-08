#include "WWSocket.h"
enum class Level {
	Fault,
	Warning,
	Info
};
template<typename... Arg>
void logger(Level lv,Arg...arg) {
	tm* now = localtime(nullptr);
	switch (lv) {
		case Level::Fault:
			cout << "[Fault] " << put_time(now, "%H:%M:%S") << ": " << ... << arg;
			break;
		case Level::Warning:
			cout << "[Warning] " << put_time(now, "%H:%M:%S") << ": " << ...<< arg;
			break;
		case Level::Info:
			cout << "[INF] " << put_time(now, "%H:%M:%S") << ": " << ...<<arg;
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
	if (!_WINSKINT)
		_INIT_WINSOCK();
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
			logger(Level::Warning,"fail in rcv for" , clientDsc.clientAddr.ipAddr," : " , WSAGetLastError());
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
		delete buf;
	}
	while (clientDsc.status == ClientStatus::Connected && rc != SOCKET_ERROR && sk.IsRunning);
	
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
					logger(Level::Warning,"fail to convert ip and port for client");
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
	for (int i{ 0 }; i < this->Clients.size(); i++) {
		SClientDesc& client = this->Clients[i];
		if (shutdown(client.clientSocket, SD_BOTH) == SOCKET_ERROR) {
			logger(Level::Warning,"fail to shut down socket: " ,client.clientAddr.ipAddr);
		}
	}
	if (closesocket(this->_SockObject) == SOCKET_ERROR) {
		throw exception("fail to close server socket");
	}
	return this->IsRunning;
}
SAddr ServerSocket::GetSAddr() {
	return this->Addr;
}
bool ServerSocket::SendTo(SClientDesc& clientDsc, SData* data) {
	if (this->IsRunning) {
		if (clientDsc.status == ClientStatus::Disconnected) {
			logger(Level::Warning, "fail to send to socket: ", clientDsc.clientAddr.ipAddr);
			return false;
		}
		int r = send(clientDsc.clientSocket, data->data, data->size, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, "fail to send to socket: ", clientDsc.clientAddr.ipAddr, " ", WSAGetLastError());
			return false;
		}
		return true;
	}
}
void ServerSocket::SendToAll(SData* data) {
	if (this->Clients.size() > 0 && this->IsRunning) {
		for (int i{ 0 }; i < this->Clients.size(); i++) {
			SClientDesc& client = this->Clients[i];
			SOCKET sk = client.clientSocket;
			int r = send(sk, data->data, data->size, 0);
			if (r == SOCKET_ERROR) {
				logger(Level::Warning,"fail to mass send to socket: ", client.clientAddr.ipAddr ," ", WSAGetLastError());
			}

		}
	}
}

//CLIENT FUNCTION DEFINITIONS:

void ClientListener(ClientSocket cs) {
	int r = 0;
	do {
		char* buf = new char[BUFFERSIZE];
		r = recv(cs._SockObject, buf, BUFFERSIZE, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, "fail in rcv for: ",WSAGetLastError());
		}
		else if (!r) {
			cs.Disconnect();
		}
		if (r > 0) {
			SData data;
			data.data = buf;
			data.size = r;
			if (cs.OnReplayEvent)
				cs.OnReplayEvent(&data);
		}
	} while (cs.IsConnected && r != SOCKET_ERROR);
}
bool ClientSocket::Connect(SAddr addr) {
	if (IsConnected)
		return true;
	if (!ValidateSAddr(addr)) {
		throw exception("invalid SAddr struct");
	}
	if (!_WINSKINT)
		_INIT_WINSOCK();
	sockaddr_in soAddr = {};
	soAddr.sin_family = AF_INET;
	int r = InetPtonA(AF_INET, addr.ipAddr.c_str(), &soAddr.sin_addr);
	if (!r)
		throw exception("fail to convert ip address of host");
	soAddr.sin_port = htons(addr.port);
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_IPV4);
	r = connect(client, (sockaddr*)&soAddr, sizeof(sockaddr));
	if (r == SOCKET_ERROR) {
		throw exception((string("fail to connect to the server socket WSERNO:") + string(to_string(WSAGetLastError()))).c_str());
	}
	this->_SockObject = client;
	IsConnected = true;
	thread listenT = thread(ClientListener, *this);
}
void ClientSocket::Disconnect() {
	if (this->IsConnected) {
		this->IsConnected = false;
		if (shutdown(this->_SockObject, SD_BOTH) == SOCKET_ERROR) {
			throw exception("fail to shutdown connection with server");
		}
		if (closesocket(this->_SockObject) == SOCKET_ERROR) {
			throw exception("fail to close socket");
		}
	}
}
bool ClientSocket::Send(SData* data) {
	if (this->IsConnected) {
		int r = send(this->_SockObject, data->data, data->size, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, "fail to send data: ", WSAGetLastError());
			return false;
		}
		return true;
	}
}
