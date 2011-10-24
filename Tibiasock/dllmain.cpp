//Tibiasock.dll by DarkstaR
//Special thanks to the authors of the following pages:

#include <windows.h>

#define OUTGOINGDATASTREAM 0x0828A08
#define OUTGOINGDATALEN 0x09B4894
#define SENDOUTGOINGPACKET 0x04FBA80

#define INCOMINGDATASTREAM 0x09B4880
#define PARSERFUNC 0x045d3f0

/* PEB & TIB */
DWORD GetThreadInfoBlockPointer()
{
	DWORD ThreadInfoBlock;
	__asm
	{
		MOV EAX, FS:[0x18]
		MOV [ThreadInfoBlock], EAX
	}
	return ThreadInfoBlock;
}
DWORD GetProcessImageBase(HANDLE process)
{
	DWORD ThreadInfoBlock = GetThreadInfoBlockPointer();
	DWORD ProcessEnviromentBlock, ImageBase;

	ReadProcessMemory(process, (LPVOID)(ThreadInfoBlock + 0x30), &ProcessEnviromentBlock, 4, NULL);
	ReadProcessMemory(process, (LPVOID)(ProcessEnviromentBlock + 0x8), &ImageBase, 4, NULL);
	return ImageBase;
}
DWORD GetProcessMainThreadId(HANDLE process)
{
	DWORD ThreadInfoBlock = GetThreadInfoBlockPointer();
	DWORD MainThreadId;

	ReadProcessMemory(process, (LPVOID)(ThreadInfoBlock + 0x24), &MainThreadId, 4, NULL);
	return MainThreadId;
}

/* THREADS */
HANDLE OpenAndSuspendThread(DWORD threadID)
{
	HANDLE thread = OpenThread((THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT), false, threadID);
	SuspendThread(thread);
	return thread;
}
void ResumeAndCloseThread(HANDLE thread)
{
	ResumeThread(thread);
	CloseHandle(thread);
}
void ExecuteRemoteCode(HANDLE process, LPVOID codeAddress, LPVOID arg)
{
	HANDLE WorkThread = CreateRemoteThread(process, NULL, NULL, (LPTHREAD_START_ROUTINE)codeAddress, arg, NULL, NULL);
	WaitForSingleObject(WorkThread, INFINITE);
	CloseHandle(WorkThread);
}

/* MAIN CODE */
DWORD Rebase(DWORD address, DWORD base)
{
	return (DWORD)(((int)address - (int)0x400000) + (int)base);
}
/* To Server */
BYTE* CreateOutgoingBuffer(BYTE* dataBuffer, int length)
{
	BYTE actualBuffer[1024];
	ZeroMemory((LPVOID)actualBuffer, 8);
	memcpy((LPVOID)&actualBuffer[8], (LPVOID)dataBuffer, length-8);
	return actualBuffer;

}

void SendPacketToServerEx(HANDLE process, BYTE* dataBuffer, int length, DWORD SendStreamData, DWORD SendStreamLength, DWORD SendPacketCall)
{
	DWORD MainThreadId = GetProcessMainThreadId(process);
	HANDLE MainThread = OpenAndSuspendThread(MainThreadId);

	int OldLength;
	BYTE OldData[1024];
	ReadProcessMemory(process, (LPVOID)SendStreamLength, &OldLength, 4, NULL);
	ReadProcessMemory(process, (LPVOID)SendStreamData, OldData, OldLength, NULL);

	length += 8;
	BYTE* actualBuffer = CreateOutgoingBuffer(dataBuffer, length);
	WriteProcessMemory(process, (LPVOID)SendStreamLength, &length, 4, NULL);
	WriteProcessMemory(process, (LPVOID)SendStreamData, actualBuffer, length, NULL);

	ExecuteRemoteCode(process, (LPVOID)SendPacketCall, (LPVOID)1);

	WriteProcessMemory(process, (LPVOID)SendStreamLength, &OldLength, 4, NULL);
	WriteProcessMemory(process, (LPVOID)SendStreamData, OldData, OldLength, NULL);

	ResumeAndCloseThread(MainThread);
}
void SendPacketToServer(HANDLE process, BYTE* dataBuffer, int length)
{
	DWORD ImageBase = GetProcessImageBase(process);
	DWORD SendStreamData = Rebase(OUTGOINGDATASTREAM, ImageBase);
	DWORD SendStreamLength = Rebase(OUTGOINGDATALEN, ImageBase);
	DWORD SendPacketCall = Rebase(SENDOUTGOINGPACKET, ImageBase);
	SendPacketToServerEx(process, dataBuffer, length, SendStreamData, SendStreamLength, SendPacketCall);
}

/* To Client */
void WriteIncomingBuffer(HANDLE process, DWORD recvStream, BYTE* data, int length, int position)
{
	DWORD DataPointer;
	WriteProcessMemory(process, (LPVOID)(recvStream + 4), &length, 4, NULL);
	WriteProcessMemory(process, (LPVOID)(recvStream + 8), &position, 4, NULL);
	
	ReadProcessMemory(process, (LPVOID)recvStream, &DataPointer, 4, NULL);
	WriteProcessMemory(process, (LPVOID)DataPointer, data, length, NULL);
}
LPVOID CreateRemoteBuffer(HANDLE process, BYTE* dataBuffer, int length)
{
	LPVOID RemoteBufferPointer = VirtualAllocEx(process, NULL, length, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(process, RemoteBufferPointer, dataBuffer, length, NULL);
	return RemoteBufferPointer;
}

void SendPacketToClientEx(HANDLE process, BYTE* dataBuffer, int length, DWORD RecvStream, DWORD ParserCall)
{
	DWORD MainThreadId = GetProcessMainThreadId(process);
	HANDLE MainThread = OpenAndSuspendThread(MainThreadId);

	DWORD DataPointer;
	int OldLength, OldPosition;
	BYTE OldDataBuffer[4096];
	ReadProcessMemory(process, (LPVOID)(RecvStream + 4), &OldLength, 4, NULL);
	ReadProcessMemory(process, (LPVOID)(RecvStream + 8), &OldPosition, 4, NULL);
	ReadProcessMemory(process, (LPVOID)RecvStream, &DataPointer, 4, NULL);
	ReadProcessMemory(process, (LPVOID)DataPointer, OldDataBuffer, OldLength, NULL);

	WriteIncomingBuffer(process, RecvStream, dataBuffer, length, 0); 
	
	BYTE CodeCave[10] = {0xB8, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xD0, 0xC3}; //MOV EAX, <DWORD> | CALL EAX | RETN
	memcpy(&CodeCave[1], &ParserCall, 4);
	LPVOID CodeCavePointer = CreateRemoteBuffer(process, CodeCave, 10);

	ExecuteRemoteCode(process, CodeCavePointer, (LPVOID)0);

	VirtualFreeEx(process, CodeCavePointer, 10, MEM_RELEASE);
	WriteIncomingBuffer(process, RecvStream, OldDataBuffer, OldLength, OldPosition); 

	ResumeAndCloseThread(MainThread);
}
void SendPacketToClient(HANDLE process, BYTE* dataBuffer, int length)
{
	DWORD ImageBase = GetProcessImageBase(process);
	DWORD RecvStream = Rebase(INCOMINGDATASTREAM, ImageBase);
	DWORD ParserCall = Rebase(PARSERFUNC, ImageBase);
	SendPacketToClientEx(process, dataBuffer, length, RecvStream, ParserCall);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}