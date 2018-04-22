#include<Windows.h>
#include<stdio.h>
#include"gui.h"
#include"gui_pe.h"
#include"Hook.h"

//header Class
class headerfilter{
private:
	WORD HeaderList[0x400];
	int Count;

public:
	headerfilter();
	bool Add(WORD Header);
	bool Check(WORD Header);
	void Clear();
};


headerfilter::headerfilter(){
	Count = 0;
}

bool headerfilter::Add(WORD Header){
	if(!Check(Header)){
		HeaderList[Count++] = Header;
		return true;
	}
	return false;
}

bool headerfilter::Check(WORD Header){
	int i;
	for(i=0; i<Count; i++){
		if(HeaderList[i] == Header){
			return true;
		}
	}
	return false;
}

void headerfilter::Clear(){
	Count = 0;
}
//

gui pe;
list packet_log;
list filter;
headerfilter fo, fr;

#define C_SENDHOOK 101
#define C_RECVHOOK 102
#define E_RETURNADDRESS 201
#define E_PACKET 202
#define E_FORMAT 203
#define B_CLEAR 301
#define B_SEND 302

#define E_FILTERHEADER 211
#define E_FILTERTYPE 212
#define B_FILTERADD 311
#define B_FILTERDELETE 312

void gui_create(HWND hWnd){
	pe.CheckBox(C_SENDHOOK, "SendHook", 110, 500, true);
	pe.CheckBox(C_RECVHOOK, "RecvHook", 220, 500, true);
	
	pe.EditBox(E_RETURNADDRESS, "", 3, 500, 100);
	pe.EditBox(E_PACKET, "", 3, 520, 550);
	pe.EditBox(E_FORMAT, "", 3, 540, 550);
	pe.Button(B_CLEAR, "Clear", 330, 500);
	pe.Button(B_SEND, "Send", 560, 520);
	
	//List Control
	packet_log.Set(hWnd, 3, 3, 594, 494);
	packet_log.AddHeader("Type", 40);
	packet_log.AddHeader("Return", 60);
	packet_log.AddHeader("Length", 50);
	packet_log.AddHeader("Packet", 400);//Format Packet
	packet_log.AddHeader("", 0);//Original Packet

	//Ignore List
	filter.Set(hWnd, 600, 3, 194, 494);
	filter.AddHeader("Type", 40);
	filter.AddHeader("Header", 120);

// bien start
	// Set ignore list.
	FILE* fp = NULL;
	if(NULL != (fp = fopen("FilterList.txt", "r")))
	{
		char row[65536] = "";
		char type[4] = "";
		char ignoreHeader[6] = "";
		while ( fgets(row, sizeof(row) - 1, fp) != NULL )
		{
			sscanf(row, "%[^,],%[^\t\r\n ,]", type, ignoreHeader);
			filter.AddItem(0, type);
			filter.AddItem(1, ignoreHeader);
			filter.MakeItemList(2, type, ignoreHeader);
		}
		fclose(fp);
	}
// bien end
	
	pe.EditBox(E_FILTERTYPE, "", 600, 500, 30);
	pe.EditBox(E_FILTERHEADER, "", 640, 500, 50);
	pe.Button(B_FILTERADD, "Add", 700, 500);
	//pe.Button(B_FILTERDELETE, "Delete", 740, 500);
}

void _stdcall AddFilter(){
	char temp1[128], temp2[128];
	int type;
	WORD Header;

	pe.GetString(E_FILTERTYPE, temp1);
	pe.GetString(E_FILTERHEADER, temp2);
	if(strcmp(temp1, "Out") == 0){
		type = 0;
	}
	else if(strcmp(temp1, "In") == 0){
		type = 1;
	}
	else{
		return;
	}
	sscanf(temp2, "@%04X", &Header);
	if(type == 0){
		if(!fo.Add(Header)){
			return;
		}
	}else if(type == 1){
		if(!fr.Add(Header)){
			return;
		}
	}
	filter.AddItem(0, temp1);
	filter.AddItem(1, temp2);

// bien
	// Add to item list.
	filter.MakeItemList(2, temp1, temp1);

	FILE* fp = NULL;
	if(NULL != (fp = fopen("FilterList.txt", "a")))
	{
		fprintf(fp, "%s,%s\r\n", temp1, temp2);
		fclose(fp);
	}
}

char ep[1024 * 4];
void gui_control(WORD wID){
	switch(wID){
	case B_SEND:
		pe.GetString(E_PACKET, ep);
		SendPacketQueue(ep);
		break;
	case B_CLEAR:
		packet_log.Clear();
		break;
	case B_FILTERADD:
		AddFilter();
		break;
	default:
		break;
	}
}

void gui_notify(HWND hWnd){
	if(hWnd == packet_log.GetHWND()){
		char Buffer[1024 * 4];
		packet_log.GetSelectedItem(1, Buffer);
		if(!Buffer[0]){
			return;
		}
		pe.SetString(E_RETURNADDRESS, Buffer);
		packet_log.GetSelectedItem(4, Buffer);
		pe.SetString(E_PACKET, Buffer);
		Buffer[5] = 0;//�I�[
		pe.SetString(E_FILTERHEADER, Buffer);
		packet_log.GetSelectedItem(3, Buffer);
		pe.SetString(E_FORMAT, Buffer);
		packet_log.GetSelectedItem(0, Buffer);
		pe.SetString(E_FILTERTYPE, Buffer);
	}
}

char Buffer[65536];
void _stdcall AddLog(DWORD Type, DWORD dwReturn, DWORD Length, BYTE *Packet, char *Format){
	if((Type == 0 && !pe.CheckBoxStatus(C_SENDHOOK)) || (Type == 1 && !pe.CheckBoxStatus(C_RECVHOOK))){
		return;
	}

// bien start
// compare ignore header.
	char header[6] = {0};
	char filterHeader[6] = {0};

	sprintf(header, "@%.04X", *(WORD *)&Packet[0 + Type * 4]);
	for(int rowsCnt = 0; rowsCnt < (int)filter.rows.size(); rowsCnt++)
	{
		sprintf(filterHeader, "%s", filter.rows.at(rowsCnt).item.at(1).c_str());
		if(memcmp(filterHeader, header, 5) == 0)
		{
			return;
		}
	}
// bien end

	if(Type == 0){
		if(fo.Check(*(WORD *)&Packet[0])){
			return;
		}
		packet_log.AddItem(0, "Out");
	}
	else{
		if(fr.Check(*(WORD *)&Packet[4])){
			return;
		}
		packet_log.AddItem(0, "In");
	}
	sprintf(Buffer, "%08X", dwReturn);
	packet_log.AddItem(1, Buffer);
	sprintf(Buffer, "%d", Length);
	packet_log.AddItem(2, Buffer);
	packet_log.AddItem(3, Format);
	sprintf(Buffer, "@%04X", *(WORD *)&Packet[0 + Type * 4]);
// bien
//	for(int i=2; i<Length; i++){
	for(int i=2; i<(int)Length; i++){
		sprintf(&Buffer[strlen(Buffer)], " %02X", Packet[i + Type * 4]);
	}
	packet_log.AddItem(4, Buffer);
}

void gui_pe(HINSTANCE hInstance){
	pe.Run(hInstance, "Packet Test", 800, 600, gui_create, gui_control, gui_notify);
}