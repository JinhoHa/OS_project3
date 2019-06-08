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
void PrintResult(int pid, int func, int aid, int demand_page, int total_process);

vector<int> physical_memory(32, -1);	// �����޸� 32�� ������
vector<vector<int>> page_table_aid;
vector<vector<int>> page_table_valid;
vector<vector<int>> page_table_frame;
map<int, int> aid_idx;					// pair(aid, idx)
vector<vector<int>> aid_info;			// [0]: page, [1]: demand_page, [2]: valid, [3]: frame, [4]: pid
queue<int> FIFOQ;						// used for FIFO policy
deque<int> LRUQ;						// used for LRU policy
int page_fault = 0;

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

	// ��ɾ� N�� ����
	while(N) {
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

		N--;
	}

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

	// ACCESS ����
	if (aid_info[idx][2] == 1) {
		// LRU ť���� aid�� �ٽ� �� �ڷ� ����
		deque<int>::iterator iter;
		for (iter = LRUQ.begin(); *iter != aid; iter++);
		LRUQ.erase(iter);
		LRUQ.push_back(aid);
		// aid ACCESS count ���� �ڵ� �־����
		printf("ACCESS ����\n");
		return 1;
	}
	// ACCESS ����, PAGE FAULT �߻�
	// �Ҵ� ���� �������� �ִ��� Ȯ��
	int demand_frame = aid_info[idx][1];
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
			// FIFO ť�� aid ����
			FIFOQ.push(aid);
			// LRU ť�� aid ����
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
	}
	printf("\n");
}