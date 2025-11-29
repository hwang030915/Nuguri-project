#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <conio.h>
#define kbhit _kbhit
#include <windows.h>
#define delay(ms) Sleep(ms)
void enable_raw_mode() {} //Windows에서는 raw mode가 기본임
void disable_raw_mode() {}
#else
#include <unistd.h>
#define delay(ms) usleep(ms*1000)
#include <termios.h>
#include <fcntl.h>

// 터미널 설정
struct termios orig_termios;

// 터미널 Raw 모드 활성화/비활성화
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


// 비동기 키보드 입력 확인
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}
#endif


// 맵 및 게임 요소 정의 (수정된 부분)
#define MAP_WIDTH 40  // 맵 너비를 40으로 변경
#define MAP_HEIGHT 20
#define MAX_STAGES 2
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

// 전역 변수
char map[MAX_STAGES][MAP_HEIGHT][MAP_WIDTH + 1];
int player_x, player_y;
int stage = 0;
int score = 0;
int life = 3; //플레이어 생명 3개 부여
int start_x, start_y;// 시작 위치 

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input);
void move_player(char input);
void move_enemies();
void check_collisions();
int kbhit();
void clrscr();
char win_getchar();
void title(); //타이틀, 게임오버, 게임클리어 함수 선언  
int gameover();
void gameclear();

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);	//출력되는 문자의 코드 페이지를 바꾸는 함수
    SetConsoleCP(CP_UTF8);			//입력되는 문자의 코드 페이지를 바꾸는 함수
#endif
    srand(time(NULL));
    enable_raw_mode();
    printf("\x1b[?25l"); //커서 깜빡임 지우기

    title(); //게임 시작 전 타이틀 화면 함수 호출

    load_maps();
    init_stage();

    clrscr();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < MAX_STAGES) {
        c = win_getchar();
        if (c == 'q') {
            game_over = 1;
            continue;
        }

        update_game(c);
        draw_game();
        delay(90);// usleep(90000) =0.09초 지연을 delay로 매핑

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < MAX_STAGES) {
                init_stage();
            }
            else {
                game_over = 1;
                gameclear(); // 단순 출력문 지우고 클리어 함수 호출 
            }
        }
    }

    disable_raw_mode();
    printf("\x1b[?25h"); // 커서 다시 보이기 
    return 0;
}

char win_getchar() {  //윈도우는 raw모드가 아니라서 getchar 사용시 
    //화면에 입력문자가 출력되고,
#ifdef _WIN32         //엔터입력 전까지 대기가 계속되는 문제 발생
    if (_kbhit()) {   //getchar대신 _getch사용
        int ch1 = _getch();

        if (ch1 == 0xE0) {
            int ch2 = _getch();

            switch (ch2) {
            case 72: return 'w';
            case 80: return 's';
            case 75: return 'a';
            case 77: return 'd';
            }
            return '\0'; //방향키가 아니면 무시
        }
        return (char)ch1; //입력한 키 반환
    }
    return '\0';//입력이 없으면 입력 없음 처리
#else
    if (kbhit()) {
        int ch = getchar();

        if (ch == '\x1b') {
            int ch1 = getchar();
            int ch2 = getchar();

            if (ch1 == '[') {
                switch (ch2) {
                case 'A': return 'w'; // Up
                case 'B': return 's'; // Down
                case 'C': return 'd'; // Right
                case 'D': return 'a'; // Left
                }
            }
            return '\0';
        }
        return (char)ch;
    }
    return '\0';
#endif
}

//화면 지우기 
void clrscr() {
    printf("\x1b[2J\x1b[H");
}

// 맵 파일 로드
void load_maps() {
    FILE* file = fopen("map.txt", "r");
    if (!file) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }
    int s = 0, r = 0;
    char line[MAP_WIDTH + 2]; // 버퍼 크기는 MAP_WIDTH에 따라 자동 조절됨
    while (s < MAX_STAGES && fgets(line, sizeof(line), file)) {
        if ((line[0] == '\n' || line[0] == '\r') && r > 0) {
            s++;
            r = 0;
            continue;
        }
        if (r < MAP_HEIGHT) {
            line[strcspn(line, "\n\r")] = 0;
            strncpy(map[s][r], line, MAP_WIDTH + 1);
            r++;
        }
    }
    fclose(file);
}


// 현재 스테이지 초기화
void init_stage() {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S') {
                player_x = x;
                player_y = y;

                start_x = x;
                start_y = y;
            }
            else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] = (Enemy){ x, y, (rand() % 2) * 2 - 1 };
                enemy_count++;
            }
            else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){ x, y, 0 };
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    printf("\x1b[H");
    printf("Stage: %d | Score: %d\n", stage + 1, score);
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");
    printf("Life : %d\n", life);

    char display_map[MAP_HEIGHT][MAP_WIDTH + 1];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char cell = map[stage][y][x];
            if (cell == 'S' || cell == 'X' || cell == 'C') {
                display_map[y][x] = ' ';
            }
            else {
                display_map[y][x] = cell;
            }
        }
    }

    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            printf("%c", display_map[y][x]);
        }
        printf("\n");
    }
}

// 게임 상태 업데이트
void update_game(char input) {
    move_player(input);
    move_enemies();
    check_collisions();
}

// 플레이어 이동 로직
void move_player(char input) {
    int next_x = player_x, next_y = player_y;
    char floor_tile = (player_y + 1 < MAP_HEIGHT) ? map[stage][player_y + 1][player_x] : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
    case 'a': next_x--; break;
    case 'd': next_x++; break;
    case 'w': if (on_ladder) next_y--; break;
    case 's': if (on_ladder && (player_y + 1 < MAP_HEIGHT) && map[stage][player_y + 1][player_x] != '#') next_y++; break;
    case ' ':
        if (!is_jumping && (floor_tile == '#' || on_ladder)) {
            is_jumping = 1;
            velocity_y = -2;
        }
        break;
    }

    if (on_ladder && (input == 'w' || input == 's')) {
        if (next_y >= 0 && next_y < MAP_HEIGHT && map[stage][next_y][player_x] != '#') {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    }
    else
    {
        if (is_jumping)
        {
            if (velocity_y < 0)
            {
                next_y = player_y - 1;
                if (next_y < 0 || (map[stage][next_y][player_x] == '#' && !on_ladder)) 
                {
                    is_jumping = 0;
                    velocity_y = 0;
                }
                else
                {
                    player_y = next_y;
                }
            }
            else if (velocity_y >= 0)
            {
                while (velocity_y > 0)
                {
                    next_y = player_y + 1;
                    if (next_y >= MAP_HEIGHT || map[stage][next_y][player_x] == '#') 
                    {
                        is_jumping = 0;
                        velocity_y = 0;
                        break;
                    }

                    player_y = next_y;
                    velocity_y--;
                }

            }

            velocity_y++;

        }
        else
        {
            if (floor_tile != '#' && floor_tile != 'H') {
                if (player_y + 1 < MAP_HEIGHT) player_y++;
                else init_stage();
            }
        }
    }

    if (player_y >= MAP_HEIGHT) {
        player_x = start_x;
        player_y = start_y;
        is_jumping = 0;
        velocity_y = 0;
    }


}


// 적 이동 로직
void move_enemies() {
    for (int i = 0; i < enemy_count; i++) {
        int next_x = enemies[i].x + enemies[i].dir;
        if (next_x < 0 || next_x >= MAP_WIDTH || map[stage][enemies[i].y][next_x] == '#' || (enemies[i].y + 1 < MAP_HEIGHT && map[stage][enemies[i].y + 1][next_x] == ' ')) {
            enemies[i].dir *= -1;
        }
        else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions() {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            score = (score > 50) ? score - 50 : 0;

            life--;

            if (life <= 0) {
                int retry = gameover();
                if (retry == 1) { //재도전 로직
                    life = 3;
                    score = 0;
                    stage = 0;
                    init_stage(); //1스테이지 맵 다시 로드
                }
                else { //종료 로직
                    disable_raw_mode();
                    printf("\x1b[?25h");
                    exit(0);

                }
            }

            else {
                // 생명 남아 있으면 시작점으로 이동
                player_x = start_x;
                player_y = start_y;

                // 점프랑 속도 값 초기화
                is_jumping = 0;
                velocity_y = 0;
            }
            return;
        }
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
        }
    }
}

void title() {
    clrscr();
    printf("\n\n");
    printf("====================================================\n");
    printf("==                                                ==\n");
    printf("==            N U G U R I   G A M E               ==\n");
    printf("==                                                ==\n");
    printf("==------------------------------------------------==\n");
    printf("==      영차영차 오늘도 모험을 떠나는 너구리      ==\n");
    printf("==      하지만 위험한 뭔가가 돌아다니고 있어      ==\n");
    printf("==------------------------------------------------==\n");
    printf("==       아무키나 눌러 게임을 시작해보세요!       ==\n");
    printf("====================================================\n");

    while (1) {
        char key = win_getchar();
        if (key != '\0') { //입력된 키가 있으면 루프 탈출 후 게임시작
            break;
        }
    }
    clrscr(); //게임 시작 전 타이틀 화면 지우기
}


int gameover() {
    clrscr();
    printf("\n\n");
    printf("====================================================\n");
    printf("==                                                ==\n");
    printf("==             G A M E    O V E R                 ==\n");
    printf("==                                                ==\n");
    printf("==------------------------------------------------==\n");
    printf("==        앗! 목숨 3개를 모두 잃었어 -ㅅ-         ==\n");
    printf("==------------------------------------------------==\n");
    printf("==             최종 점수 : %-5d                  ==\n", score);
    printf("==                                                ==\n");
    printf("==            다시 도전해볼까? (r)                ==\n");
    printf("==                나가기 (q)                      ==\n");
    printf("====================================================\n");

    while (1) {
        char c = win_getchar();
        if (c == 'r' || c == 'R') {
            return 1; //재도전 1 반환
        }
        if (c == 'q' || c == 'Q') {
            return 0;
        }
    }
    return 0;
}

void gameclear() {
    clrscr();
    printf("\n\n");
    printf("====================================================\n");
    printf("==                                                ==\n");
    printf("==            G A M E    C L E A R                ==\n");
    printf("==                                                ==\n");
    printf("==------------------------------------------------==\n");
    printf("==         너구리가 무사히 도착했어 !             ==\n");
    printf("==     대단해 모든 스테이지 클리어 성공~!         ==\n");
    printf("==------------------------------------------------==\n");
    printf("==             최종 점수 : %-5d                  ==\n", score);
    printf("==                                                ==\n");
    printf("==               나가기 (q)                       ==\n");
    printf("====================================================\n");

    while (1) {
        char c = win_getchar();
        if (c == 'q' || c == 'Q') {
            disable_raw_mode();
            printf("\x1b[?25h");
            exit(0);

        }
    }

}