#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <WinSock2.h>
#include <thread>
#include <ctime>
#include <iomanip>
#include <WS2tcpip.h>

using namespace std;
#define MAXCLIENT 10
#define BUFFERSIZE 2048
#pragma comment(lib,"Ws2_32.lib")
class ClientSocket;
enum class ClientStatus {
	Connected,
	Disconnected
};
struct SClientDesc {
	thread* ClientListenerThread = nullptr;
	ClientStatus status;
	SOCKET clientSocket;
	SAddr clientAddr;
	thread clientListenThread;
};
struct SData {
	char* data;
	size_t size;
};
class SAddr {
public:
	string ipAddr = "";
	int port = 0;
	int AddrType = AF_INET; //no definition for AF_INET6
};
typedef void (*OnClientStatusChanged)(SClientDesc&, ClientStatus);
typedef void (*OnReplay)(SClientDesc&, SData*);

void _INIT_WINSOCK();
bool _WINSKINT = false;
class ServerSocket {
public:
	bool IsRunning;
	bool Run();
	bool Shutdown();
	explicit ServerSocket(SAddr);
	struct SAddr GetSAddr();
	bool SendTo(SClientDesc&,SData*);
	void SendToAll(SData*);
	OnClientStatusChanged OnClientEvent;
	OnReplay OnReplayEvent;
	vector<SClientDesc&> Clients;
private:
	SAddr Addr = { 0 };
protected:
	SOCKET _SockObject = INVALID_SOCKET;
};
class ClientSocket {
public:
	bool Connect(SAddr);
	bool Disconnect();
	bool Send(SData*);
	OnReplay OnReplayEvent;
	friend class ServerSocket;
private:
	bool isConnected;
	bool _validate_SAddr(); 
	bool _shutdown();
protected:
	SOCKET _SockObject = INVALID_SOCKET;
};

bool ValidateSAddr(SAddr& in) {
	struct in_addr addr;
	if (in.ipAddr == "" || InetPtonA(AF_INET, in.ipAddr.c_str(), &addr))
		return false;
	if (in.port > 65535 || in.port < 1) {
		return false;
	}
	(void)addr;
	return true;
}
