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

vector<int> physical_memory(32, -1);	// 물리메모리 32개 프레임
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
	int replPolicy;							// 페이지 교체 알고리즘
	int total_process;						// 프로세스의 수
	int N;									// 명령어의 수
	int pid, func, aid, demand_page;

	scanf("%d", &replPolicy);
	scanf("%d", &total_process);
	scanf("%d", &N);
	// 가상메모리 프로세스별 각 64개 페이지
	page_table_aid.assign(total_process, vector<int>(64, -1));
	page_table_valid.assign(total_process, vector<int>(64, -1));
	page_table_frame.assign(total_process, vector<int>(64, -1));

	// 명령어 N번 수행
	while(N) {
		scanf("%d %d %d", &pid, &func, &aid);			// 명령어 읽음
		// function = 1 : 페이지 테이블 할당
		if (func) {
			scanf("%d", &demand_page);
			PageTableAlloc(pid, aid, demand_page);
		}
		// function = 0 : 메모리 접근
		else {
			MemoryAccess(pid, aid, replPolicy);
		}
		
		// 결과 출력
		PrintResult(pid, func, aid, demand_page, total_process);

		N--;
	}

	return 0;
}

int PageTableAlloc(int pid, int aid, int demand_page)
{
	int available_page = 0;			// 할당 가능페이지 count
	int page = 0;					// 할당 시작페이지
	for (int i = 0; i < 64; i++) {
		// i페이지에 할당가능
		if (page_table_aid[pid][i] == -1) {
			available_page++;
		}
		// i페이지에 할당불가능
		else {
			available_page = 0;
			page = i + 1;
		}
		// 연속할당 가능함
		if (available_page == demand_page) {
			break;
		}
		// page table에 연속할당 불가능한 경우 -1 리턴
		if (i == 63) return -1;
	}
	// 연속된 페이지에 할당
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
	int page = aid_info[idx][0];			// aid가 존재하는 페이지

	// ACCESS 성공
	if (aid_info[idx][2] == 1) {
		// LRU 큐에서 aid를 다시 맨 뒤로 삽입
		deque<int>::iterator iter;
		for (iter = LRUQ.begin(); *iter != aid; iter++);
		LRUQ.erase(iter);
		LRUQ.push_back(aid);
		// aid ACCESS count 증가 코드 넣어야함
		printf("ACCESS 성공\n");
		return 1;
	}
	// ACCESS 실패, PAGE FAULT 발생
	// 할당 가능 프레임이 있는지 확인
	int demand_frame = aid_info[idx][1];
	int available_frame = 0;
	int frame = 0;
	for (int i = 0; i < 32; i++) {
		// i프레임에 할당 가능
		if (physical_memory[i] == -1) {
			available_frame++;
		}
		// i프레임에 할당 불가능
		else {
			available_frame = 0;
			frame = i + 1;
		}
		// 프레임에 연속 할당 가능, 프레임 할당
		if (available_frame == demand_frame) {
			// 할당작업
			// aid info 수정
			aid_info[idx][2] = 1;
			aid_info[idx][3] = frame;
			// FIFO 큐에 aid 삽입
			FIFOQ.push(aid);
			// LRU 큐에 aid 삽입
			LRUQ.push_back(aid);
			for (int i = page, j = frame; i < page + demand_frame; i++, j++) {
				page_table_valid[pid][i] = 1;
				page_table_frame[pid][i] = j;
				physical_memory[j] = aid;
			}
			break;
		}
		// 할당불가, release작업 필요
		if (i == 31) {
			ReleaseFrame(replPolicy);
			// release 후 프레임 할당 가능 여부를 다시 확인
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
		// 할당 해제 할 aid를 FIFO 큐에서 꺼냄
		released_aid = FIFOQ.front();
		FIFOQ.pop();
	}
	// CASE 1: LRU
	else if (replPolicy == 1) {
		// 할당 해제 할 aid를 LRU 큐에서 꺼냄
		released_aid = LRUQ.front();
		LRUQ.pop_front();
	}

	// physical memeory에서 할당 해제, page table 수정
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
	// 해제한 aid_info 수정
	aid_info[idx][2] = 0;		// invalid
	aid_info[idx][3] = -1;		// frame No.
								//디버그 출력
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