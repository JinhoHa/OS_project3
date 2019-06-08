// author: 2017147594 ����ȣ
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <deque>
using namespace std;

int PageTableAlloc(int pid, int aid, int demand_page);
int MemoryAccess(int pid, int aid, int replPolicy);
int ReleaseFrame(int replPolicy);
int UpdateReference(void);
void PrintResult(int pid, int func, int aid, int demand_page, int total_process);

vector<int> physical_memory(32, -1);	// �����޸� 32�� ������
vector<vector<int>> page_table_aid;
vector<vector<int>> page_table_valid;
vector<vector<int>> page_table_frame;
vector<vector<bool>> page_table_R;
vector<vector<unsigned char>> reference_byte;
map<int, int> aid_idx;					// pair(aid, idx)
vector<vector<int>> aid_info;			// [0]: page, [1]: demand_page, [2]: valid, [3]: frame, [4]: pid
queue<int> FIFOQ;						// used for FIFO policy
deque<int> LRUQ;						// used for LRU policy
int page_fault = 0;
int time_interval;

int main()
{
	int replPolicy;							// ������ ��ü �˰���
	int total_process;						// ���μ����� ��
	int N;									// ��ɾ��� ��
	int pid, func, aid, demand_page;

	scanf("%d", &replPolicy);
	scanf("%d", &total_process);
	scanf("%d", &N);
	// ����޸� ���μ����� �� 64�� ������
	page_table_aid.assign(total_process, vector<int>(64, -1));
	page_table_valid.assign(total_process, vector<int>(64, -1));
	page_table_frame.assign(total_process, vector<int>(64, -1));
	page_table_R.assign(total_process, vector<bool>(64, 0));
	reference_byte.assign(total_process, vector<unsigned char>(64, 0));

	time_interval = 8;
	// ��ɾ� N�� ����
	while(N) {
		time_interval--;
		printf("time interval : %d\n", time_interval);
		// ��ɾ� 8�� ���ึ�� update reference byte & clear reference bit
		if (time_interval == 7) {
			UpdateReference();
		}
		scanf("%d %d %d", &pid, &func, &aid);			// ��ɾ� ����
		// function = 1 : ������ ���̺� �Ҵ�
		if (func) {
			scanf("%d", &demand_page);
			PageTableAlloc(pid, aid, demand_page);
		}
		// function = 0 : �޸� ����
		else {
			MemoryAccess(pid, aid, replPolicy);
		}
		
		// ��� ���
		PrintResult(pid, func, aid, demand_page, total_process);

		if (!time_interval) {
			time_interval = 8;
		}
		N--;
	}
	printf("page fault = %d\n", page_fault);

	return 0;
}

int PageTableAlloc(int pid, int aid, int demand_page)
{
	int available_page = 0;			// �Ҵ� ���������� count
	int page = 0;					// �Ҵ� ����������
	for (int i = 0; i < 64; i++) {
		// i�������� �Ҵ簡��
		if (page_table_aid[pid][i] == -1) {
			available_page++;
		}
		// i�������� �Ҵ�Ұ���
		else {
			available_page = 0;
			page = i + 1;
		}
		// �����Ҵ� ������
		if (available_page == demand_page) {
			break;
		}
		// page table�� �����Ҵ� �Ұ����� ��� -1 ����
		if (i == 63) return -1;
	}
	// ���ӵ� �������� �Ҵ�
	for (int i = page; i < page + demand_page; i++) {
		page_table_aid[pid][i] = aid;
		page_table_valid[pid][i] = 0;
	}
	int idx = aid_idx.size();
	aid_idx.insert(pair<int, int>(aid, idx));
	vector<int> v = { page, demand_page, 0, -1, pid };
	aid_info.push_back(v);

	return 0;
}

int MemoryAccess(int pid, int aid, int replPolicy)
{
	int idx = aid_idx.find(aid)->second;
	int page = aid_info[idx][0];			// aid�� �����ϴ� ������
	int demand_frame = aid_info[idx][1];
	// R��Ʈ 1�� �ٲ� (SAMPLED LRU)
	for (int i = page; i < page + demand_frame; i++) {
		page_table_R[pid][i] = 1;
	}

	// ACCESS ����
	if (aid_info[idx][2] == 1) {
		// LRU ť���� aid�� �ٽ� �� �ڷ� ���� (LRU)
		deque<int>::iterator iter;
		for (iter = LRUQ.begin(); *iter != aid; iter++);
		LRUQ.erase(iter);
		LRUQ.push_back(aid);
		// aid ACCESS count ���� �ڵ� �־����
		printf("ACCESS ����\n");
		return 1;
	}
	// ACCESS ����, PAGE FAULT �߻�
	page_fault++;
	// �Ҵ� ���� �������� �ִ��� Ȯ��
	int available_frame = 0;
	int frame = 0;
	for (int i = 0; i < 32; i++) {
		// i�����ӿ� �Ҵ� ����
		if (physical_memory[i] == -1) {
			available_frame++;
		}
		// i�����ӿ� �Ҵ� �Ұ���
		else {
			available_frame = 0;
			frame = i + 1;
		}
		// �����ӿ� ���� �Ҵ� ����, ������ �Ҵ�
		if (available_frame == demand_frame) {
			// �Ҵ��۾�
			// aid info ����
			aid_info[idx][2] = 1;
			aid_info[idx][3] = frame;
			// FIFO ť�� aid ���� (FIFO)
			FIFOQ.push(aid);
			// LRU ť�� aid ���� (LRU)
			LRUQ.push_back(aid);
			for (int i = page, j = frame; i < page + demand_frame; i++, j++) {
				page_table_valid[pid][i] = 1;
				page_table_frame[pid][i] = j;
				physical_memory[j] = aid;
			}
			break;
		}
		// �Ҵ�Ұ�, release�۾� �ʿ�
		if (i == 31) {
			ReleaseFrame(replPolicy);
			// release �� ������ �Ҵ� ���� ���θ� �ٽ� Ȯ��
			i = -1;
			available_frame = 0;
			frame = 0;
		}
	}
	
	return 0;
}

int ReleaseFrame(int replPolicy)
{
	int released_aid;
	// CASE 0: FIFO
	if (replPolicy == 0) {
		// �Ҵ� ���� �� aid�� FIFO ť���� ����
		released_aid = FIFOQ.front();
		FIFOQ.pop();
	}
	// CASE 1: LRU
	else if (replPolicy == 1) {
		// �Ҵ� ���� �� aid�� LRU ť���� ����
		released_aid = LRUQ.front();
		LRUQ.pop_front();
	}
	// CASE 2: SAMPLED LRU
	else if (replPolicy == 2) {
		printf("released_aid ����\n");
		// aid�� ���� �ͺ��� ���ʷ� reference byte ��, ���� ���� ���� ��ü
		map<int, int>::iterator iter;
		int smallest_aid;
		unsigned char smallest_reference = 0b11111111;
		for (iter = aid_idx.begin(); iter != aid_idx.end(); iter++) {
			int idx = iter->second;
			// valid�� aid �߿����� ����
			if (!aid_info[idx][2]) { continue; }
			unsigned char reference = reference_byte[aid_info[idx][4]][aid_info[idx][0]];
			if (reference < smallest_reference) {
				smallest_aid = iter->first;
				smallest_reference = reference;
			}
		}
		released_aid = smallest_aid;
		printf("released_aid Ż��\n");
		printf("released_aid : %d\n", released_aid);
	}

	printf("table ���� ����\n");
	// physical memeory���� �Ҵ� ����, page table ����
	int idx = aid_idx.find(released_aid)->second;
	int page = aid_info[idx][0];
	int demand_page = aid_info[idx][1];
	int frame = aid_info[idx][3];
	int pid = aid_info[idx][4];
	for (int i = page, j = frame; i < page + demand_page; i++, j++) {
		//page_table_frame[pid][i] = -1;
		page_table_valid[pid][i] = 0;
		physical_memory[j] = -1;
	}
	// ������ aid_info ����
	aid_info[idx][2] = 0;		// invalid
	aid_info[idx][3] = -1;		// frame No.
								//����� ���
	printf("==Released== (AID : %d)\n", released_aid);

	return 0;
}

int UpdateReference(void)
{
	printf("UPDATE REFERENCE\n");
	int total_process = page_table_R.size();
	// reference byte�� ���������� 1��Ʈ�� �ű�� R ��Ʈ�� ���� 0, 1�� ���ʿ� ä��
	// reference bit�� ��� 0���� clear
	for (int pid = 0; pid < total_process; pid++) {
		for (int i = 0; i < 64; i++) {
			reference_byte[pid][i] >> 1;
			if (page_table_R[pid][i]) {
				page_table_R[pid][i] = 0;
				reference_byte[pid][i] += 0b1000000;
			}
		}
	}

	return 0;
}

void PrintResult(int pid, int func, int aid, int demand_page, int total_process)
{

	if (func) {
		printf("* Input : Pid [%d] Function [ALLOCATION] Alloc ID [%d] Page Num[%d]\n", pid, aid, demand_page);
	}
	else {
		int idx = aid_idx.find(aid)->second;
		printf("* Input : Pid [%d] Function [ACCESS] Alloc ID [%d] Page Num[%d]\n", pid, aid, aid_info[idx][1]);
	}
	printf("%-30s", ">> Physical Memory : ");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0) { printf("|"); }
		if (physical_memory[i] == -1) { printf("-"); }
		else { printf("%d", physical_memory[i]); }
	}
	printf("|\n");
	for (int pid = 0; pid < total_process; pid++) {
		printf(">> pid(%d) %-20s", pid, "Page Table(AID) : ");
		for (int i = 0; i < 64; i++) {
			if (i % 4 == 0) { printf("|"); }
			if (page_table_aid[pid][i] == -1) { printf("-"); }
			else { printf("%d", page_table_aid[pid][i]); }
		}
		printf("|\n");
		printf(">> pid(%d) %-20s", pid, "Page Table(Valid) : ");
		for (int i = 0; i < 64; i++) {
			if (i % 4 == 0) { printf("|"); }
			if (page_table_valid[pid][i] == -1) { printf("-"); }
			else { printf("%d", page_table_valid[pid][i]); }
		}
		printf("|\n");
		/**********debug****************/
		printf(">> pid(%d) %-20s", pid, "Page Table(R) : ");
		for (int i = 0; i < 64; i++) {
			if (i % 4 == 0) { printf("|"); }
			page_table_R[pid][i] ? printf("1") : printf("0");
		}
		printf("|\n");
		/*******************************/
	}

	printf("\n");
}