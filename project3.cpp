// author: 2017147594 ����ȣ
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <deque>
#include <set>
using namespace std;

int PageTableAlloc(int pid, int aid, int demand_page);
int MemoryAccess(int pid, int aid, int replPolicy);
int ReleaseFrame(int replPolicy);
int UpdateReference(void);
void PrintResult(int pid, int func, int aid, int demand_page, int total_process);

vector<vector<int>> instruction;		// ��ɾ�
vector<int> physical_memory(32, -1);	// �����޸� 32�� ������
vector<vector<int>> page_table_aid;
vector<vector<int>> page_table_valid;
vector<vector<bool>> page_table_R;
vector<vector<unsigned char>> reference_byte;
map<int, int> aid_idx;					// pair(aid, idx)
vector<vector<int>> aid_info;			// [0]: page, [1]: demand_page, [2]: valid, [3]: demand_frame, [4]: pid, [5]: frame, [6]: count
queue<int> FIFOQ;						// used for FIFO policy
deque<int> LRUQ;						// used for LRU policy
int page_fault = 0;
int time_interval;						// used for SAMPLED LRU
int current_instruction;

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
	page_table_R.assign(total_process, vector<bool>(64, 0));
	reference_byte.assign(total_process, vector<unsigned char>(64, 0));
	instruction.assign(N, vector<int>(4));
	// ��ɾ� �Է¹���
	for (int i = 0; i < N; i++) {
		scanf("%d %d %d", &pid, &func, &aid);
		instruction[i][0] = pid;
		instruction[i][1] = func;
		instruction[i][2] = aid;
		if (func) {
			scanf("%d", &demand_page); 
			instruction[i][3] = demand_page;
		}
	}

	time_interval = 8;
	// ��ɾ� N�� ����
	for (int i = 0; i < N; i++) {
		current_instruction = i;
		time_interval--;
		// ��ɾ� 8�� ���ึ�� update reference byte & clear reference bit
		if (time_interval == 7) {
			UpdateReference();
		}
		// ��ɾ� ����
		pid = instruction[i][0];
		func = instruction[i][1];
		aid = instruction[i][2];
		demand_page = instruction[i][3];
		// function = 1 : ������ ���̺� �Ҵ�
		if (func) {
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
		if (i == 63) { return -1; }
	}
	// ���ӵ� �������� �Ҵ�
	for (int i = page; i < page + demand_page; i++) {
		page_table_aid[pid][i] = aid;
		page_table_valid[pid][i] = 0;
	}
	// demand frame ���
	int demand_frame = 32;
	while (demand_page <= (demand_frame / 2)) {
		demand_frame /= 2;
	}
	// aid_idx�� ���� aid -> idx ��ȯ�� aid_info[idx]�� ���� ����Ȯ��
	int idx = aid_idx.size();
	aid_idx.insert(pair<int, int>(aid, idx));
	vector<int> v = { page, demand_page, 0, demand_frame, pid, -1, 0 };
	aid_info.push_back(v);

	return 0;
}

int MemoryAccess(int pid, int aid, int replPolicy)
{
	int idx = aid_idx.find(aid)->second;
	int page = aid_info[idx][0];			// aid�� �����ϴ� ������
	int demand_page = aid_info[idx][1];
	int demand_frame = aid_info[idx][3];
	// R��Ʈ 1�� �ٲ� (SAMPLED LRU)
	for (int i = page; i < page + demand_page; i++) {
		page_table_R[pid][i] = 1;
	}
	// COUNT ���� (LFU, MFU)
	aid_info[idx][6] += 1;

	// ACCESS ����
	if (aid_info[idx][2] == 1) {
		// LRU ť���� aid�� �ٽ� �� �ڷ� ���� (LRU)
		deque<int>::iterator iter;
		for (iter = LRUQ.begin(); *iter != aid; iter++);
		LRUQ.erase(iter);
		LRUQ.push_back(aid);
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
			i = frame + demand_frame - 1;
			frame = i + 1;
		}
		// �����ӿ� ���� �Ҵ� ����, ������ �Ҵ�
		if (available_frame == demand_frame) {
			// �Ҵ��۾�
			// aid info ����
			aid_info[idx][2] = 1;
			aid_info[idx][5] = frame;
			// FIFO ť�� aid ���� (FIFO)
			FIFOQ.push(aid);
			// LRU ť�� aid ���� (LRU)
			LRUQ.push_back(aid);
			// ������ ���̺�, ������ �޸� ����
			for (int i = page; i < page + demand_page; i++) {
				page_table_valid[pid][i] = 1;
			}
			for (int i = frame; i < frame + demand_frame; i++) {
				physical_memory[i] = aid;
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
	int released_aid;	// ��ü�� aid
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
	}
	// CASE 3: LFU
	else if (replPolicy == 3) {
		// aid�� ���� �ͺ��� ���ʷ� count ��, ���� ���� ���� ��ü
		map<int, int>::iterator iter;
		int least_aid;
		int least_count = 987654321;
		int count;
		for (iter = aid_idx.begin(); iter != aid_idx.end(); iter++) {
			int idx = iter->second;
			if (!aid_info[idx][2]) { continue; }
			count = aid_info[idx][6];
			if (count < least_count) {
				least_aid = iter->first;
				least_count = count;
			}
		}
		released_aid = least_aid;
	}
	else if (replPolicy == 4) {
		map<int, int>::iterator iter;
		int most_aid;
		int most_count = -1;
		int count;
		for (iter = aid_idx.begin(); iter != aid_idx.end(); iter++) {
			int idx = iter->second;
			if (!aid_info[idx][2]) { continue; }
			count = aid_info[idx][6];
			if (count > most_count) {
				most_aid = iter->first;
				most_count = count;
			}
		}
		released_aid = most_aid;
	}
	else if (replPolicy == 5) {
		set<int> valid_aid_set;
		set<int>::iterator iter;
		int aid_to_use;
		// ���� valid ������ aid�� set�� �߰�
		for (int i = 0; i < 32; i++) {
			if (physical_memory[i] != -1) {
				valid_aid_set.insert(physical_memory[i]);
			}
		}
		for (iter = valid_aid_set.begin(); iter != valid_aid_set.end(); iter++) {
		}
		// ������ ���� aid�� ������� set���� ����
		// set�� �ϳ��� ���ų�, ������ instruction�� ������ ������ �ݺ�
		for (int i = current_instruction + 1; i < instruction.size(); i++) {
			if (valid_aid_set.size() == 1) { break; }
			aid_to_use = instruction[i][2];
			iter = valid_aid_set.find(aid_to_use);
			if (iter != valid_aid_set.end()) {
				valid_aid_set.erase(iter);
			}
		}
		// ������ ���� aid�� release��
		// ������ ���� ��� aid�� ���� ��
		iter = valid_aid_set.begin();
		released_aid = *iter;
		valid_aid_set.clear();
	}

	// physical memeory���� �Ҵ� ����, page table ����
	int idx = aid_idx.find(released_aid)->second;
	int page = aid_info[idx][0];
	int demand_page = aid_info[idx][1];
	int demand_frame = aid_info[idx][3];
	int pid = aid_info[idx][4];
	int frame = aid_info[idx][5];
	for (int i = page; i < page + demand_page; i++) {
		page_table_valid[pid][i] = 0;
	}
	for (int i = frame; i < frame + demand_frame; i++) {
		physical_memory[i] = -1;
	}
	// ������ aid_info ����
	aid_info[idx][2] = 0;		// invalid
	aid_info[idx][5] = -1;		// frame No.
	// COUNT �ʱ�ȭ (LFU, MFU)
	aid_info[idx][6] = 0;

	return 0;
}

int UpdateReference(void)
{
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
	}
	printf("\n");
}