#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <ctime>
#include <iomanip>
#include <WS2tcpip.h>
#include <chrono>
#include <WinSock2.h>
#include <cstdarg>

#pragma comment(lib,"Ws2_32.lib")
using namespace std;

#ifndef TDF
#define TDF

#define MAXCLIENT 10
#define BUFFERSIZE 2048

enum class ClientStatus {
	Connected,
	Disconnected
};
struct SClientDesc {
	thread* ClientListenerThread = nullptr;
	ClientStatus status;
	SOCKET clientSocket;
	struct SAddr* clientAddr;
	thread clientListenThread;
};
struct SData {
	char* data;
	int size;
};
struct SAddr {
	string ipAddr = "";
	int port = 0;
	int AddrType = AF_INET;
};

typedef void (*OnClientStatusChanged)(SClientDesc*, ClientStatus);
typedef void (*OnServerReplay)(SClientDesc*, SData*);
typedef void (*OnReplay)(SData*);

class ServerSocket {
public:
	ServerSocket();
	bool IsRunning;
	bool Run();
	bool Shutdown();
	bool DisconnectFrom(SClientDesc*);
	explicit ServerSocket(SAddr);
	struct SAddr GetSAddr();
	bool SendTo(SClientDesc&, SData*);
	void SendToAll(SData*);
	OnClientStatusChanged OnClientEvent;
	OnServerReplay OnReplayEvent;
	vector<SClientDesc*> Clients;
private:
	struct SAddr Addr;
protected:
	SOCKET _SockObject = INVALID_SOCKET;
};
class ClientSocket {
public:
	bool Connect(SAddr);
	void Disconnect();
	bool Send(SData*);
	OnReplay OnReplayEvent;
	bool IsConnected = false;
	SOCKET _SockObject = INVALID_SOCKET;
};
#endif