#include"AirMemory.h"

namespace Air{
	DWORD ReadAob(char *Aob, BYTE *b, bool *mask);
	int CreateMemory(BYTE *b, char *Code);
}

DWORD Air::ReadAob(char *Aob, BYTE *b, bool *mask){
	DWORD dwSize = 0;
	int i, j;
	BYTE temp;
	BYTE rb;
	i = 0;
	j = 0;

	while(Aob[j]){
		rb = (BYTE)Aob[j];
		if(i%2 == 0){
			temp = 4;//high
		}
		else{
			temp = 0;//low
		}

		if(rb >= '0' && rb <= '9'){
			b[i/2] += (rb - '0') << temp;
			i++;
		}
		else if(rb >= 'A' && rb <= 'F'){
			b[i/2] += (rb - 'A' + 0x0A) << temp;
			i++;
		}
		else if(rb >= 'a' && rb <= 'f'){
			b[i/2] += (rb - 'a' + 0x0A) << temp;
			i++;
		}
		else if(rb == '?'){
			b[i/2] = 0xFF;
			mask[i/2] = true;
			i++;
		}
		j++;
	}

	return (i%2 == 0) ? (i/2) : -1;
}

int Air::CreateMemory(BYTE *b, char *Code){
	BYTE wb, temp;
	int i, j;
	i = 0;
	j = 0;
	while(Code[j]){
		wb = (BYTE)Code[j];
		if(i%2 == 0){
			temp = 4;//high
		}
		else{
			temp = 0;//low
		}

		if(wb >= '0' && wb <= '9'){
			b[i/2] += (wb - '0') << temp;
			i++;
		}
		else if(wb >= 'A' && wb <= 'F'){
			b[i/2] += (wb - 'A' + 0x0A) << temp;
			i++;
		}
		else if(wb >= 'a' && wb <= 'f'){
			b[i/2] += (wb - 'a' + 0x0A) << temp;
			i++;
		}
		j++;
	}

	return (i%2 == 0) ? (i/2) : -1;
}

//AirMemory

AirMemory::AirMemory(){
	BaseAddress = NULL;
	Memory_Start = NULL;
	Memory_End = NULL;
	MemoryDump = NULL;
	IsDLL = false;
	MemoryList.Next = NULL;
}

void AirMemory::Init(char *ModuleName){
	HMODULE hModule = NULL;
	MODULEINFO ModInfo;
	HANDLE hProcess = GetCurrentProcess();
	IMAGE_NT_HEADERS *nth;

	if(ModuleName){//DLL
		while(!(hModule = GetModuleHandleA(ModuleName))){
			Sleep(100);
		}
		GetModuleInformation(hProcess, hModule, &ModInfo, sizeof(MODULEINFO));
		Memory_Start = (DWORD)ModInfo.lpBaseOfDll;
		Memory_End = (DWORD)ModInfo.lpBaseOfDll + (DWORD)ModInfo.SizeOfImage;
		IsDLL = true;
	}
	else{//Client
		hModule = GetModuleHandleA(NULL);
		nth = (IMAGE_NT_HEADERS *)((DWORD)hModule + PIMAGE_DOS_HEADER(hModule)->e_lfanew);
		Memory_Start = (DWORD)nth->OptionalHeader.ImageBase;
		Memory_End = (DWORD)nth->OptionalHeader.ImageBase + (DWORD)nth->OptionalHeader.SizeOfImage;
	}
	BaseAddress = Memory_Start;
}


void AirMemory::CreateMemoryDump(){
	BYTE *NewMemory = (BYTE *)malloc(Memory_End - Memory_Start);
	RtlCopyMemory(NewMemory, (DWORD *)Memory_Start, Memory_End - Memory_Start);
	MemoryDump = (DWORD)NewMemory;
}

bool AirMemory::FullAccess(DWORD Address, DWORD Size){
	if(VirtualProtect((DWORD *)Address, Size, PAGE_EXECUTE_READWRITE, &OldProtect)){
		OldAddr = Address;
		OldSize = Size;
		return true;
	}
	return false;
}

bool AirMemory::RestoreProtect(){
	DWORD old;
	return VirtualProtect((DWORD *)OldAddr, OldSize, OldProtect, &old);
}

DWORD AirMemory::AobScan(char *Aob, int Result, DWORD StartAddress, DWORD EndAddress){
	BYTE Memory[MaxSize] = {0};
	bool mask[MaxSize] = {0};
	DWORD Size = Air::ReadAob(Aob, Memory, mask);
	DWORD i, j, ResultCount = 0;

	if(Size < 1){
		return 0;
	}
	
	StartAddress = StartAddress? StartAddress: Memory_Start;
	EndAddress = EndAddress? EndAddress: Memory_End;

	__try{
		for(i=StartAddress; i<EndAddress; i++){
			for(j=0; j<Size; j++){
				if(mask[j]){
					continue;
				}
				if(Memory[j] != *(BYTE *)(i + j)){
					break;
				}
			}
			if(j == Size){
				ResultCount++;
				if(ResultCount >= Result){
					return i;
				}
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		return 0;
	}
	return 0;
}

DWORD AirMemory::GetAbsoluteAddress(DWORD Address){
	if(IsDLL){
		if(Address < BaseAddress){
			return (Address + BaseAddress);
		}
	}
	return Address;
}

void AirMemory::AddBackupMemory(DWORD Address, void *Memory, DWORD Size){
	BackupMemory *p = (BackupMemory *)malloc(sizeof(BackupMemory));

	p->Address = Address;
	p->Size = Size;
	p->Memory = malloc(Size);
	MemoryCopy((DWORD)p->Memory, (void *)p->Address, p->Size);//Backup Memory
	if(Memory){
		MemoryCopy(p->Address, Memory, p->Size);//Write Memory
	}

	if(!MemoryList.Next){
		p->Next = NULL;
		MemoryList.Next = p;
	}
	else{
		p->Next = MemoryList.Next;
		MemoryList.Next = p;
	}
}

bool AirMemory::DeleteBackupMemory(DWORD Address){
	BackupMemory *p, *d, *pp;
	pp = &MemoryList;
	for(p=MemoryList.Next; p; p=p->Next){
		if(p->Address == Address){
			d = p;
			MemoryCopy(d->Address, d->Memory, d->Size);//Restore Memory
			pp->Next = p->Next;
			free(d->Memory);
			free(d);
			return true;
		}
		pp = p;
	}
	return false;
}

void AirMemory::MemoryCopy(DWORD Address, void *Memory, DWORD Size){
	__try{
		if(FullAccess(Address, Size)){
			RtlCopyMemory((void *)Address, Memory, Size);
			RestoreProtect();
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
	}
	return;
}

bool AirMemory::MemoryWriter(DWORD Address, char *Code){
	BYTE Memory[MaxSize] = {0};
	DWORD Size = Air::CreateMemory(Memory, Code);

	if(Size < 1){
		return false;
	}

	AddBackupMemory(GetAbsoluteAddress(Address), Memory, Size);
	return true;
}


bool AirMemory::RestoreMemory(DWORD Address){
	return DeleteBackupMemory(GetAbsoluteAddress(Address));
}

bool AirMemory::WriteHook(DWORD Address, WORD OpCode, void *Function, DWORD *RetAddr, DWORD AddNop){
	DWORD Size;
	if(OpCode < 0x100){
		Size = 5;
	}
	else{
		Size = 6;
	}

	Address = GetAbsoluteAddress(Address);

	FullAccess(Address, Size + AddNop);
	AddBackupMemory(Address, NULL, Size + AddNop);
	__try{
		if(OpCode < 0x100){
			*(BYTE *)(Address) = OpCode;
			*(DWORD *)(Address + 1) = (DWORD)Function - Address - Size;
		}
		else{
			*(WORD *)(Address) = OpCode;
			*(DWORD *)(Address + 2) = (DWORD)Function - Address - Size;
		}
		for(int i = 0; i<AddNop; i++){
			*(BYTE *)(Address + Size + i) = 0x90;
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		return false;
	}
	RestoreProtect();

	if(RetAddr){
		*RetAddr = Address + Size + AddNop;
	}
	return true;
}

void AirMemory::GetDumpInfo(DWORD *MS, DWORD *ME, DWORD *MD){
	*MS = Memory_Start;
	*ME = Memory_End;
	*MD = MemoryDump;
}

DWORD AirMemory::GetBaseAddress(){
	return BaseAddress;
}

void AirMemory::PointerHook(DWORD Pointer, void *Function, DWORD *OldFunction){
	Pointer = GetAbsoluteAddress(Pointer);

	FullAccess(Pointer, 4);//v1.01
	AddBackupMemory(Pointer, NULL, 4);
	*OldFunction = *(DWORD *)Pointer;
	*(DWORD *)Pointer = (DWORD)Function;
	RestoreProtect();//v1.01
}

DWORD AirMemory::AutoVMHook(DWORD Address, void *Function, DWORD *RetAddr, DWORD MinAddr){
	DWORD i, VMSection, VMSection_Ret;

	Address = GetAbsoluteAddress(Address);

	for(i=Address; ;i++){
		if(*(BYTE *)i == JMP){
			VMSection = *(DWORD *)(i + 1) + i + 5;
			if((VMSection > Memory_Start) && (VMSection < Memory_End)){
				break;
			}
		}
	}

	for(i=VMSection; ;i++){
		switch(*(BYTE *)i){
		case JMP:
			VMSection_Ret =  *(DWORD *)(i + 1) + i + 5;
			if((VMSection_Ret > Memory_Start) && (VMSection_Ret < Memory_End)){
				if(MinAddr && i < MinAddr){
					i = VMSection_Ret;
					break;
				}
				WriteHook(i, JMP, Function);
				*RetAddr = VMSection_Ret;
				return i;
			}
			break;

		case CALL:
			VMSection_Ret =  *(DWORD *)(i + 1) + i + 5;
			if((VMSection_Ret > Memory_Start) && (VMSection_Ret < Memory_End)){
				if(MinAddr && i < MinAddr){
					i = VMSection_Ret;
					break;
				}
				WriteHook(i, CALL, Function);
				*RetAddr = VMSection_Ret;
				return i;
			}
			break;

		default:
			break;
		}
	}
}

void WriteJMP(DWORD Prev, DWORD Next){
	*(BYTE *)Prev = 0xE9;
	*(DWORD *)(Prev + 1) = Next - Prev - 5;
}

void WriteCALL(DWORD Prev, DWORD Next){
	*(BYTE *)Prev = 0xE8;
	*(DWORD *)(Prev + 1) = Next - Prev - 5;
}

bool AirMemory::FunctionHook(char *ProcName, void *Function, DWORD *RetAddr){
	DWORD ExportFunction, i;
	void *NewMem = malloc(7);

	if(!IsDLL){
		return false;
	}

	ExportFunction = (DWORD)GetProcAddress((HMODULE)BaseAddress, ProcName);

	if(!ExportFunction){
		return false;
	}

	FullAccess((DWORD)NewMem, 7);

	if(FullAccess(ExportFunction - 5, 7)){
		if(*(WORD *)ExportFunction == 0xFF8B || *(BYTE *)ExportFunction == 0x6A){
			for(i=(ExportFunction - 5); i<ExportFunction; i++){
				if(*(BYTE *)i != 0x90 && *(BYTE *)i != 0xCC){
					RestoreProtect();
					return false;
				}
			}
			*RetAddr = (DWORD)NewMem;
			*(WORD *)((DWORD)NewMem) = *(WORD *)ExportFunction;
			WriteJMP((DWORD)NewMem + 2, ExportFunction + 2);
			WriteJMP(ExportFunction - 5, (DWORD)Function);
			*(WORD *)(ExportFunction) = 0xF9EB;
			RestoreProtect();
			return true;
		}
	}

	return false;
}

void AirMemory::ArgumentsHook(DWORD Address, DWORD OverWrite, void *HookFunction){
	DWORD NewMem = (DWORD)malloc(128);
	Address = GetAbsoluteAddress(Address);
	FullAccess((DWORD)NewMem, 128);
	*(WORD *)(NewMem) = 0xC48B;//mov eax,esp
	*(BYTE *)(NewMem + 2) = 0x60;//pushad
	*(BYTE *)(NewMem + 3) = 0x50;//push eax
	WriteCALL(NewMem + 4, (DWORD)HookFunction);//call
	*(BYTE *)(NewMem + 9) = 0x61;//popad
	MemoryCopy(NewMem + 10, HookFunction, OverWrite);
	WriteCALL(NewMem + 10 + OverWrite, (DWORD)HookFunction + OverWrite);//jmp
}