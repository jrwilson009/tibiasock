#include <iostream>
#include <tchar.h>
#include <windows.h>
#include "Packet.h"
#define DESIRED_ACCESS PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ

using namespace std;

typedef void (*_SendPacket)(HANDLE process, BYTE* dataBuffer, int length);

DWORD FindProcessByWindowName(char* windowName)
{
	DWORD procID = NULL;
	HWND window = FindWindowA(NULL, windowName);

	if (window)
		GetWindowThreadProcessId(window, &procID);

	return procID;
}

int _tmain(int argc, _TCHAR* argv[])
{
	_SendPacket SendPacketToServer;
	_SendPacket SendPacketToClient;
	HINSTANCE TibiaSockLib = LoadLibraryA("TibiaSock.dll");
	SendPacketToServer = (_SendPacket)GetProcAddress(TibiaSockLib, "SendPacketToServer");
	SendPacketToClient = (_SendPacket)GetProcAddress(TibiaSockLib, "SendPacketToClient");


	WORD procID = NULL;
	while (!procID)
	{
		system("Cls");
		cout << "Please start Tibia and login to begin the test." << endl;
		system("Pause");
		procID = FindProcessByWindowName("Tibia");
	}

	HANDLE process = OpenProcess(DESIRED_ACCESS, false, procID);

	for (int i = 0; i < 1000; i++)
	{
		srand(GetTickCount());

		Packet* p = new Packet();
		p->AddByte(0x96);
		p->AddByte(0x01);
		p->AddString("Testing OI");
		SendPacketToServer(process, p->GetRawPacket(), p->GetRawSize());
		delete p;

		p = new Packet();
		p->AddByte(0xB4);
		p->AddByte(0x11);
		p->AddString("Testing Incoming Injection");
		SendPacketToClient(process, p->GetRawPacket(), p->GetRawSize());
		delete p;

		Sleep(rand() % 900 + 700);
	}

	CloseHandle(process);
	return 0;
}

