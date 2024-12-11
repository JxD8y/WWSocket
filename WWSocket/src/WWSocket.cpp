#define _CRT_SECURE_NO_WARNINGS
#include "..\include\WWSocket.h"

enum class Level {
	Fault,
	Warning,
	Info
};
void logger(Level lv, string log) {
	auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	string time = ctime(&time_t);
	time.pop_back();
	switch (lv) {
		case Level::Fault:
			cout << "[Fault] " << time << ": "<<log<<endl;
			break;
		case Level::Warning:
			cout << "[Warning] " << time << ": "<<log<<endl;
			break;
		case Level::Info:
			cout << "[INF] " << time << ": " << log<< endl;
			break;
	}
}
bool _WINSKINT = false;
void _INIT_WINSOCK() {
	if (!_WINSKINT) {
		WSADATA wdata;
		_WINSKINT = !(WSAStartup(MAKEWORD(2, 2), &wdata));
		if (!_WINSKINT) {
			throw exception("fail to initiate WINSOCK");
		}

	}
}
void CleanUp() {
	if (_WINSKINT) {
		int r = WSACleanup();
		if (r == SOCKET_ERROR) {
			logger(Level::Fault, "fail to cleanup WS.");
		}
		_WINSKINT = false;
	}
}
bool ValidateSAddr(SAddr& in) {
	struct in_addr addr;
	if (in.ipAddr == "" || InetPtonA(AF_INET, in.ipAddr.c_str(), &addr) != 1)
		return false;
	if (in.port > 65535 || in.port < 1) {
		return false;
	}
	(void)addr;
	return true;
}
bool ConvertSocketAddrIPPORT(const sockaddr_in* sockad, char* ip, int* port) {
	*port = ntohs(sockad->sin_port);
	if (inet_ntop(AF_INET, &(sockad->sin_addr), ip, INET_ADDRSTRLEN) == NULL)
		return false;
	return true;
}
ServerSocket::ServerSocket() {
	this->IsRunning = false;
	this->OnClientEvent = nullptr;
	this->OnReplayEvent = nullptr;
}
ServerSocket::ServerSocket(SAddr serverAddr) {
	this->IsRunning = false;
	this->OnClientEvent = nullptr;
	this->OnReplayEvent = nullptr;
	if (!ValidateSAddr(serverAddr)) {
		CleanUp();
		throw exception("Invalid Address struct");
	}
	if (!_WINSKINT)
		_INIT_WINSOCK();
	this->Addr = serverAddr;
	this->_SockObject = socket(serverAddr.AddrType, SOCK_STREAM, 0);
	if (this->_SockObject == INVALID_SOCKET) {
		CleanUp();
		throw exception("fail to create socket");
	}
}
void Server_ClientListener(SClientDesc* clientDsc, ServerSocket sk) {
	int rc = 0;
	do {
		char* buf = new char[BUFFERSIZE];
		ZeroMemory(buf, BUFFERSIZE);
		SOCKET s = clientDsc->clientSocket;
		rc = recv(clientDsc->clientSocket, buf, BUFFERSIZE, 0);
		if (rc == SOCKET_ERROR) {
			logger(Level::Warning, (string("fail in rcv for") + clientDsc->clientAddr->ipAddr + string( " : ") +  to_string(WSAGetLastError())));
		}
		else if (!rc) {
			if (sk.OnClientEvent) {
				sk.OnClientEvent(clientDsc, ClientStatus::Disconnected);
			}
			clientDsc->status = ClientStatus::Disconnected;
			sk.Clients.erase(remove(sk.Clients.begin(), sk.Clients.end(), clientDsc));
		}
		if (rc > 0) {
			SData data;
			data.data = buf;
			data.size = rc;
			if (sk.OnReplayEvent)
				sk.OnReplayEvent(clientDsc, &data);
		}
		delete[] buf;
	} while (clientDsc->status == ClientStatus::Connected && rc != SOCKET_ERROR && sk.IsRunning);

}
bool ServerSocket::Run() {
	if (this->IsRunning)
		return true;
	if (!_WINSKINT)
		_INIT_WINSOCK();
	if (this->Addr.port == 0 && this->Addr.ipAddr == "") {
		CleanUp();
		throw exception("No address specified to bind to");
	}
	if (this->_SockObject == INVALID_SOCKET) {
		CleanUp();
		throw exception("Socket was not created.");
	}
	//binding
	sockaddr_in sockad;
	sockad.sin_port = htons(this->Addr.port);
	sockad.sin_family = this->Addr.AddrType;
	int result = InetPtonA(this->Addr.AddrType, this->Addr.ipAddr.c_str(), &sockad.sin_addr);
	if (!result) {
		CleanUp();
		throw exception("fail to conver ip address");
	}
	result = bind(this->_SockObject, (sockaddr*)&sockad, sizeof(sockaddr_in));
	if (result == SOCKET_ERROR) {
		CleanUp();
		throw exception("fail to bind");//use wsagetlasterror
	}
	if (!(this->OnClientEvent) || !(this->OnReplayEvent)) {
		CleanUp();
		throw exception("no ClientEvent listener passed");
	}
	result = listen(this->_SockObject, 20);
	if (result == SOCKET_ERROR) {
		CleanUp();
		throw exception("fail to put socket in listen mode");
	}
	SOCKET _sk = this->_SockObject;
	this->IsRunning = true;
	thread ListenThread = thread([this, _sk]() {
		while (this->IsRunning && this->Clients.size() <= MAXCLIENT) {
			sockaddr_in clientA;
			int clientAddr_len = sizeof(sockaddr);
			SOCKET clientSocket = accept(_sk, (sockaddr*)&clientA, &clientAddr_len);
			cout << WSAGetLastError() << endl;
			if (clientSocket != INVALID_SOCKET) {
				SAddr* clientAddr = new SAddr;
				int port = 0;
				char* ipString = new char[INET_ADDRSTRLEN];
				if (!ConvertSocketAddrIPPORT(&clientA, ipString, &port)) {
					logger(Level::Warning, "fail to convert ip and port for client");
					clientAddr->ipAddr = "NOADDR";
					clientAddr->port = 0;
				}
				clientAddr->ipAddr = ipString;
				clientAddr->port = port;
				SClientDesc* clientDSC = new SClientDesc;
				clientDSC->clientSocket = clientSocket;
				clientDSC->clientAddr = clientAddr;
				clientDSC->status = ClientStatus::Connected;
				this->Clients.push_back(clientDSC);
				this->OnClientEvent(clientDSC, ClientStatus::Connected);
				thread clistenThread = thread(Server_ClientListener, clientDSC, *this);
				clientDSC->ClientListenerThread = &clistenThread;
				clistenThread.detach();
			}
		}
		});
	ListenThread.detach();
}
bool ServerSocket::Shutdown() {
	this->IsRunning = false;
	for (int i{ 0 }; i < this->Clients.size(); i++) {
		SClientDesc* client = this->Clients[i];
		if (shutdown(client->clientSocket, SD_BOTH) == SOCKET_ERROR) {
			logger(Level::Warning, (string("fail to shut down socket: ")+ client->clientAddr->ipAddr));
		}
		delete client;
	}
	if (closesocket(this->_SockObject) == SOCKET_ERROR) {
		CleanUp();
		throw exception("fail to close server socket");
	}
	return this->IsRunning;
}
bool ServerSocket::DisconnectFrom(SClientDesc* dsc) {
	if (shutdown(dsc->clientSocket, SD_BOTH) == SOCKET_ERROR) {
		logger(Level::Warning, (string("fail to shut down socket: ") + dsc->clientAddr->ipAddr));
		return false;
	}
	return true;
}
SAddr ServerSocket::GetSAddr() {
	return this->Addr;
}
bool ServerSocket::SendTo(SClientDesc& clientDsc, SData* data) {
	if (this->IsRunning) {
		if (clientDsc.status == ClientStatus::Disconnected) {
			logger(Level::Warning, string("fail to send to socket: ") + clientDsc.clientAddr->ipAddr);
			return false;
		}
		int r = send(clientDsc.clientSocket, data->data, data->size, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, string("fail to send to socket: ")+ clientDsc.clientAddr->ipAddr + string(" ")+ to_string(WSAGetLastError()));
			return false;
		}
		return true;
	}
}
void ServerSocket::SendToAll(SData* data) {
	if (this->Clients.size() > 0 && this->IsRunning) {
		for (int i{ 0 }; i < this->Clients.size(); i++) {
			SClientDesc* client = this->Clients[i];
			SOCKET sk = client->clientSocket;
			int r = send(sk, data->data, data->size, 0);
			if (r == SOCKET_ERROR) {
				logger(Level::Warning, string("fail to mass send to socket: ")+ client->clientAddr->ipAddr+string( " ") + to_string(WSAGetLastError()));
			}

		}
	}
}

//CLIENT FUNCTION DEFINITIONS:

void ClientListener(ClientSocket cs) {
	int r = 0;
	do {
		char* buf = new char[BUFFERSIZE];
		ZeroMemory(buf, BUFFERSIZE);
		r = recv(cs._SockObject, buf, BUFFERSIZE, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, string("fail in rcv for: ")+ to_string( WSAGetLastError()));
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
		delete[] buf;
	} while (cs.IsConnected && r != SOCKET_ERROR);
}
bool ClientSocket::Connect(SAddr addr) {
	if (IsConnected)
		return true;
	if (!ValidateSAddr(addr)) {
		CleanUp();
		throw exception("invalid SAddr struct");
	}
	if (!_WINSKINT)
		_INIT_WINSOCK();
	sockaddr_in soAddr = {};
	soAddr.sin_family = AF_INET;
	int r = InetPtonA(AF_INET, addr.ipAddr.c_str(), &soAddr.sin_addr);
	if (!r) {
		CleanUp();
		throw exception("fail to convert ip address of host");
	}
	soAddr.sin_port = htons(addr.port);
	SOCKET client = socket(AF_INET, SOCK_STREAM, 0);
	r = connect(client, (sockaddr*)&soAddr, sizeof(sockaddr));
	if (r == SOCKET_ERROR) {
		CleanUp();
		throw exception((string("fail to connect to the server socket WSERNO:") + string(to_string(WSAGetLastError()))).c_str());
	}
	this->_SockObject = client;
	IsConnected = true;
	thread listenT = thread(ClientListener, *this);
	listenT.detach();
}
void ClientSocket::Disconnect() {
	if (this->IsConnected) {
		this->IsConnected = false;
		if (shutdown(this->_SockObject, SD_BOTH) == SOCKET_ERROR) {
			CleanUp();
			throw exception("fail to shutdown connection with server");
		}
		if (closesocket(this->_SockObject) == SOCKET_ERROR) {
			CleanUp();
			throw exception("fail to close socket");
		}
	}
}
bool ClientSocket::Send(SData* data) {
	if (this->IsConnected) {
		int r = send(this->_SockObject, data->data, data->size, 0);
		if (r == SOCKET_ERROR) {
			logger(Level::Warning, string("fail to send data: ")+ to_string(WSAGetLastError()));
			return false;
		}
		return true;
	}
}
