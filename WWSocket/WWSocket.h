
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <ctime>
#include <iomanip>
#include <WS2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib,"Ws2_32.lib")
using namespace std;
#define MAXCLIENT 10
#define BUFFERSIZE 2048
class ClientSocket;
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
	OnServerReplay OnReplayEvent;
	vector<SClientDesc*> Clients;
private:
	struct SAddr Addr = { 0 };
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
