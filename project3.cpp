// author: 2017147594 하진호
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

vector<vector<int>> instruction;		// 명령어
vector<int> physical_memory(32, -1);	// 물리메모리 32개 프레임
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
	page_table_R.assign(total_process, vector<bool>(64, 0));
	reference_byte.assign(total_process, vector<unsigned char>(64, 0));
	instruction.assign(N, vector<int>(4));
	// 명령어 입력받음
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
	// 명령어 N번 수행
	for (int i = 0; i < N; i++) {
		current_instruction = i;
		time_interval--;
		// 명령어 8개 실행마다 update reference byte & clear reference bit
		if (time_interval == 7) {
			UpdateReference();
		}
		// 명령어 정보
		pid = instruction[i][0];
		func = instruction[i][1];
		aid = instruction[i][2];
		demand_page = instruction[i][3];
		// function = 1 : 페이지 테이블 할당
		if (func) {
			PageTableAlloc(pid, aid, demand_page);
		}
		// function = 0 : 메모리 접근
		else {
			MemoryAccess(pid, aid, replPolicy);
		}
		
		// 결과 출력
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
		if (i == 63) { return -1; }
	}
	// 연속된 페이지에 할당
	for (int i = page; i < page + demand_page; i++) {
		page_table_aid[pid][i] = aid;
		page_table_valid[pid][i] = 0;
	}
	// demand frame 계산
	int demand_frame = 32;
	while (demand_page <= (demand_frame / 2)) {
		demand_frame /= 2;
	}
	// aid_idx를 통해 aid -> idx 변환후 aid_info[idx]를 통해 정보확인
	int idx = aid_idx.size();
	aid_idx.insert(pair<int, int>(aid, idx));
	vector<int> v = { page, demand_page, 0, demand_frame, pid, -1, 0 };
	aid_info.push_back(v);

	return 0;
}

int MemoryAccess(int pid, int aid, int replPolicy)
{
	int idx = aid_idx.find(aid)->second;
	int page = aid_info[idx][0];			// aid가 존재하는 페이지
	int demand_page = aid_info[idx][1];
	int demand_frame = aid_info[idx][3];
	// R비트 1로 바꿈 (SAMPLED LRU)
	for (int i = page; i < page + demand_page; i++) {
		page_table_R[pid][i] = 1;
	}
	// COUNT 증가 (LFU, MFU)
	aid_info[idx][6] += 1;

	// ACCESS 성공
	if (aid_info[idx][2] == 1) {
		// LRU 큐에서 aid를 다시 맨 뒤로 삽입 (LRU)
		deque<int>::iterator iter;
		for (iter = LRUQ.begin(); *iter != aid; iter++);
		LRUQ.erase(iter);
		LRUQ.push_back(aid);
		return 1;
	}
	// ACCESS 실패, PAGE FAULT 발생
	page_fault++;
	// 할당 가능 프레임이 있는지 확인
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
			i = frame + demand_frame - 1;
			frame = i + 1;
		}
		// 프레임에 연속 할당 가능, 프레임 할당
		if (available_frame == demand_frame) {
			// 할당작업
			// aid info 수정
			aid_info[idx][2] = 1;
			aid_info[idx][5] = frame;
			// FIFO 큐에 aid 삽입 (FIFO)
			FIFOQ.push(aid);
			// LRU 큐에 aid 삽입 (LRU)
			LRUQ.push_back(aid);
			// 페이지 테이블, 피지컬 메모리 수정
			for (int i = page; i < page + demand_page; i++) {
				page_table_valid[pid][i] = 1;
			}
			for (int i = frame; i < frame + demand_frame; i++) {
				physical_memory[i] = aid;
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
	int released_aid;	// 교체할 aid
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
	// CASE 2: SAMPLED LRU
	else if (replPolicy == 2) {
		// aid가 작은 것부터 차례로 reference byte 비교, 가장 작은 것을 교체
		map<int, int>::iterator iter;
		int smallest_aid;
		unsigned char smallest_reference = 0b11111111;
		for (iter = aid_idx.begin(); iter != aid_idx.end(); iter++) {
			int idx = iter->second;
			// valid한 aid 중에서만 선택
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
		// aid가 작은 것부터 차례로 count 비교, 가장 작은 것을 교체
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
		// 현재 valid 상태인 aid를 set에 추가
		for (int i = 0; i < 32; i++) {
			if (physical_memory[i] != -1) {
				valid_aid_set.insert(physical_memory[i]);
			}
		}
		for (iter = valid_aid_set.begin(); iter != valid_aid_set.end(); iter++) {
		}
		// 다음에 사용될 aid를 순서대로 set에서 제거
		// set에 하나만 남거나, 마지막 instruction에 도달할 때까지 반복
		for (int i = current_instruction + 1; i < instruction.size(); i++) {
			if (valid_aid_set.size() == 1) { break; }
			aid_to_use = instruction[i][2];
			iter = valid_aid_set.find(aid_to_use);
			if (iter != valid_aid_set.end()) {
				valid_aid_set.erase(iter);
			}
		}
		// 마지막 남은 aid가 release됨
		// 여러개 남은 경우 aid가 작은 것
		iter = valid_aid_set.begin();
		released_aid = *iter;
		valid_aid_set.clear();
	}

	// physical memeory에서 할당 해제, page table 수정
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
	// 해제한 aid_info 수정
	aid_info[idx][2] = 0;		// invalid
	aid_info[idx][5] = -1;		// frame No.
	// COUNT 초기화 (LFU, MFU)
	aid_info[idx][6] = 0;

	return 0;
}

int UpdateReference(void)
{
	int total_process = page_table_R.size();
	// reference byte를 오른쪽으로 1비트씩 옮기고 R 비트에 따라 0, 1을 왼쪽에 채움
	// reference bit를 모두 0으로 clear
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