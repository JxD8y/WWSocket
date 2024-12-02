#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;

#pragma comment(lib,"Ws2_32.lib")
class ClientSocket;
enum class ClientStatus {
	Connected,
	Disconnected
};

struct SData {
	char* data;
	size_t size;
};
struct SAddr {
	string ipAddr = "";
	int port = 0;
	int AddrType = AF_INET; //no definition for AF_INET6
};
typedef void (*OnClientStatusChanged)(ClientSocket, ClientStatus);
typedef void (*OnReplay)(ClientSocket, SData*);

void _INIT_WINSOCK();
bool _WINSKINT = false;
class ServerSocket {
public:
	bool Run();
	bool Shutdown();
	explicit ServerSocket(SAddr);
	struct SAddr GetSAddr();
	bool SendTo(ClientSocket&,SData*);
	bool SendToAll(SData*);
	OnClientStatusChanged OnClientEvent;
	OnReplay OnReplayEvent;
private:
	bool _validate_SAddr();
	bool _shutdown();
	SAddr Addr = { 0 };
	bool IsRunning;
	vector<ClientSocket> Clients;
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
