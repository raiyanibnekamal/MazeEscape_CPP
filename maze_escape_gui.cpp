/*
    MAZE ESCAPE - Windows GUI Edition (Win32 + GDI)
    -------------------------------------------------
    A real colored window (no console/terminal). Click buttons or use
    WASD / Arrow keys to move. No external libraries needed -- only
    the Windows API (comes with MinGW / Visual Studio out of the box).

    Demonstrates: graphs (maze as adjacency list), BFS (hint path),
    Dijkstra (enemy chase), stack (undo), queue-based logging.

    Visual style matches the HTML/JS "dungeon" edition: dark stone panels,
    rounded cells, a torch-colored glow on the current room, a dashed gold
    exit outline, a gradient HP bar, and a dimmed modal overlay for the
    start/game-over screens.
*/

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#define WINVER       0x0600
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <deque>
#include <stack>
#include <queue>
#include <algorithm>
#include <random>
#include <ctime>
#include <sstream>

#pragma comment(lib, "msimg32")   // AlphaBlend / GradientFill

using namespace std;

// ---------------------------------------------------------------------------
// Constants / theme colors
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ROWS/COLS/CELL are runtime (not compile-time-const) because of the
// difficulty picker on the start screen -- picking Medium/Large calls
// RecomputeLayout() to rederive every region below from the new grid size,
// instead of anything here ever being hardcoded/guessed.
// ---------------------------------------------------------------------------
int ROWS = 5, COLS = 5, N = ROWS * COLS;
int CELL = 64;
static const int GRID_X = 24, GRID_Y = 90; // grid's top-left corner never moves

int GRID_RIGHT, GRID_BOTTOM;
int CTRL_X, CTRL_W = 120, CTRL_RIGHT;
int PANEL_X, PANEL_W = 368, PANEL_TOP, PANEL_BOTTOM;
int CLIENT_W, CLIENT_H;

// Modal overlay box (start screen / game-over screen), centered in the
// window. The start screen is taller because it also holds the difficulty
// picker.
static const int OVERLAY_BOX_W = 360;
static const int OVERLAY_BOX_H_START = 260, OVERLAY_BOX_H_END = 210;

struct DifficultyPreset{ int rows, cols, cell; const char* label; };
// cell size is chosen so rows*cell stays ~320px for every preset -- the
// window doesn't need to change size (or barely does) between difficulties.
static const DifficultyPreset DIFFICULTIES[3] = {
    {5,5,64,"Small (5x5)"},
    {7,7,46,"Medium (7x7)"},
    {9,9,36,"Large (9x9)"},
};
int g_difficulty = 0; // index into DIFFICULTIES; 0 = default/easy

void RecomputeLayout(){
    N = ROWS*COLS;
    GRID_RIGHT   = GRID_X + COLS*CELL;
    GRID_BOTTOM  = GRID_Y + ROWS*CELL;
    CTRL_X       = GRID_RIGHT + 24;
    CTRL_RIGHT   = CTRL_X + CTRL_W;
    PANEL_X      = CTRL_RIGHT + 24;
    PANEL_TOP    = GRID_Y;
    PANEL_BOTTOM = GRID_BOTTOM + 70;
    CLIENT_W     = PANEL_X + PANEL_W + 20;
    CLIENT_H     = PANEL_BOTTOM + 20;
}

#define COL_BG        RGB(15,13,11)
#define COL_PANEL     RGB(26,22,19)
#define COL_PANEL2    RGB(42,36,32)
#define COL_CELL      RGB(42,36,32)
#define COL_VISITED   RGB(36,31,26)
#define COL_CURRENT   RGB(224,138,46)
#define COL_CURRENT_GLOW RGB(74,46,20)
#define COL_EXIT      RGB(212,175,55)
#define COL_DANGER    RGB(163,49,42)
#define COL_HINT      RGB(92,122,82)
#define COL_TEXT      RGB(232,220,196)
#define COL_TEXTDIM   RGB(168,154,128)
#define COL_TORCH     RGB(244,185,87)
#define COL_WALL      RGB(6,5,4)
// COL_WALL (above) is intentionally near-black -- it's used only for the
// thin cell outline. Blocked passages need to stand out clearly against
// both the near-black background AND the cell fill, so they get their own
// bright, warm "lit stone/mortar" color instead.
#define COL_WALLBLOCK RGB(158,132,96)

mt19937 rng((unsigned)time(nullptr));
int idOf(int x,int y){return y*COLS+x;}
pair<int,int> coordOf(int id){return {id%COLS, id/COLS};}

struct Item{ string name; int value; };
static const vector<Item> ITEM_POOL = {
    {"Torch",10},{"Health Potion",15},{"Golden Key",50},{"Shield",20},{"Map Fragment",25}
};
static const vector<string> ENEMY_NAMES = {"Goblin","Skeleton","Shadow Wraith"};

struct Enemy{ string name; int room; int power; };
struct LeaderboardEntry{ string name; int score; bool won; };

// ---------------------------------------------------------------------------
// Maze / pathfinding (same algorithms as console edition)
// ---------------------------------------------------------------------------

// Randomized DFS backtracker (iterative, explicit stack -- vector<int> stk
// standing in for a call stack) builds a spanning tree over the grid graph,
// then a handful of extra random edges are added to create loops.
// Complexity: O(V+E) for the DFS pass (V=N=25 rooms, E<=4 per room), plus
// O(extra) bounded random-edge attempts (guarded so it can't loop forever).
vector<set<int>> generateMaze(){
    vector<set<int>> adj(N);
    vector<bool> visited(N,false);
    vector<int> stk; stk.push_back(0); visited[0]=true;
    auto addEdge=[&](int a,int b){ adj[a].insert(b); adj[b].insert(a); };
    while(!stk.empty()){
        int cur = stk.back();
        auto [x,y] = coordOf(cur);
        vector<int> nb;
        if(x>0 && !visited[idOf(x-1,y)]) nb.push_back(idOf(x-1,y));
        if(x<COLS-1 && !visited[idOf(x+1,y)]) nb.push_back(idOf(x+1,y));
        if(y>0 && !visited[idOf(x,y-1)]) nb.push_back(idOf(x,y-1));
        if(y<ROWS-1 && !visited[idOf(x,y+1)]) nb.push_back(idOf(x,y+1));
        if(!nb.empty()){
            shuffle(nb.begin(), nb.end(), rng);
            int next = nb[0];
            addEdge(cur,next); visited[next]=true; stk.push_back(next);
        } else stk.pop_back();
    }
    int extra = N/5, guard=0;
    uniform_int_distribution<int> distN(0,N-1);
    while(extra>0 && guard<2000){
        guard++;
        int a=distN(rng), b=distN(rng);
        auto [ax,ay]=coordOf(a); auto [bx,by]=coordOf(b);
        if((abs(ax-bx)+abs(ay-by))==1 && !adj[a].count(b)){ addEdge(a,b); extra--; }
    }
    return adj;
}

// Breadth-first search over the unweighted room graph -- guarantees the
// shortest (fewest-rooms) path since every edge has the same "cost".
// Complexity: O(V+E) -- each room enqueued/visited once, each edge scanned
// once. parent[] doubles as a reverse-linked-list for path reconstruction.
vector<int> bfsPath(const vector<set<int>>& adj,int start,int goal){
    queue<int> q; q.push(start);
    vector<bool> vis(N,false); vector<int> parent(N,-2);
    vis[start]=true; parent[start]=-1;
    while(!q.empty()){
        int cur=q.front(); q.pop();
        if(cur==goal) break;
        for(int nb: adj[cur]) if(!vis[nb]){ vis[nb]=true; parent[nb]=cur; q.push(nb); }
    }
    if(!vis[goal]) return {};
    vector<int> path; int cur=goal;
    while(cur!=-1){ path.push_back(cur); cur=parent[cur]; }
    reverse(path.begin(), path.end());
    return path;
}

// Dijkstra's algorithm (binary-heap priority_queue as the min-heap) from the
// enemy's room to the player's room; returns the FIRST step of that
// shortest path, i.e. one turn of a chase. All edges have weight 1 here
// (equivalent to BFS in this specific case), but it's written as true
// Dijkstra so it keeps working if rooms ever get weighted passages.
// Complexity: O(E log V) with a binary heap (V=N rooms, E<=4 per room).
int enemyNextStep(const vector<set<int>>& adj,int enemyRoom,int targetRoom){
    if(enemyRoom==targetRoom) return enemyRoom;
    vector<int> dist(N, INT_MAX); vector<int> parent(N,-2);
    dist[enemyRoom]=0;
    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> pq;
    pq.push({0,enemyRoom});
    while(!pq.empty()){
        auto [d,cur]=pq.top(); pq.pop();
        if(d>dist[cur]) continue;
        if(cur==targetRoom) break;
        for(int nb: adj[cur]){
            int nd=d+1;
            if(nd<dist[nb]){ dist[nb]=nd; parent[nb]=cur; pq.push({nd,nb}); }
        }
    }
    if(dist[targetRoom]==INT_MAX) return enemyRoom;
    int cur=targetRoom;
    while(parent[cur]!=enemyRoom && parent[cur]>=0) cur=parent[cur];
    return cur;
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
struct GameState{
    string playerName;
    vector<set<int>> adj;
    int exitRoom=N-1;
    map<int,string> roomItem;
    vector<Enemy> enemies;
    int currentRoom=0;
    int health=100, score=0;
    vector<string> inventory;
    stack<int> moveHistory;
    deque<string> logDeque;
    set<int> visitedRooms;
    bool running=false;
    bool started=false;
    vector<int> hintPath;
    int hintsUsed=0;
};
GameState state;
vector<LeaderboardEntry> leaderboard;

// ---------------------------------------------------------------------------
// Sound effects. Beep() is a plain Win32 call (kernel32, declared by
// windows.h already) -- no winmm/-lwinmm and no audio asset files needed.
// It plays synchronously, but each call here is short (<=180ms total) so it
// doesn't noticeably freeze this turn-based game's UI thread.
// ---------------------------------------------------------------------------
void SfxMove()    { Beep(440, 40); }                    // step
void SfxBlocked() { Beep(180, 70); }                    // bumped into a wall
void SfxPickup()  { Beep(880, 60); Beep(1320, 70); }    // rising two-tone
void SfxDanger()  { Beep(150, 150); }                   // enemy hit you
void SfxWin()     { Beep(659,90); Beep(880,90); Beep(1047,160); }  // little fanfare
void SfxLose()    { Beep(300,180); Beep(180,260); }

void logAction(const string& msg){
    state.logDeque.push_back(msg);
    if(state.logDeque.size()>6) state.logDeque.pop_front();
}
string describeRoom(int id){
    if(id==state.exitRoom) return "Sunlight pours in -- this is the EXIT!";
    if(id==0) return "You wake up on cold stone. The maze begins here.";
    return "A dusty stone chamber. Torches flicker on the walls.";
}
void newGame(const string& name){
    state = GameState();
    state.playerName = name;
    state.adj = generateMaze();
    state.currentRoom = 0;
    state.visitedRooms.insert(0);
    state.running = true;
    state.started = true;

    uniform_int_distribution<int> roomDist(1, N-2);
    set<int> used;
    for(auto& it: ITEM_POOL){
        int r,tries=0;
        do{ r=roomDist(rng); tries++; } while((used.count(r)||r==state.exitRoom) && tries<50);
        state.roomItem[r]=it.name; used.insert(r);
    }
    for(auto& nm: ENEMY_NAMES){
        int r; do{ r=roomDist(rng);} while(r==state.exitRoom);
        state.enemies.push_back({nm,r,10});
    }
    logAction("A new maze awaits, " + name + ".");
}
// Dijkstra-style single BFS-with-weights step for each enemy (all edges have
// weight 1 here, so this degrades to BFS, but enemyNextStep is written as a
// general Dijkstra so it would still work if edges ever got weighted).
// Complexity per enemy per turn: O(E log V) with the binary-heap priority
// queue inside enemyNextStep; O(enemies * E log V) total for this function.
void moveEnemies(){
    bool hit=false;
    for(auto& e: state.enemies){
        e.room = enemyNextStep(state.adj, e.room, state.currentRoom);
        if(e.room==state.currentRoom){
            state.health -= e.power;
            logAction(e.name + " catches you! -" + to_string(e.power) + " HP");
            hit=true;
        }
    }
    
    if(hit) SfxDanger();
    if(state.health<=0){ state.health=0; state.running=false; }
}
void endBookkeeping(){
    bool won = (state.currentRoom==state.exitRoom && state.health>0);
    leaderboard.push_back({state.playerName, state.score, won});
    sort(leaderboard.begin(), leaderboard.end(), [](auto&a,auto&b){
        if(a.score!=b.score) return a.score>b.score;
        return a.won && !b.won; // tie-break: a win outranks a loss at the same score
    });
}
// O(1) check against the precomputed exit room id.
void checkWin(){
    if(state.currentRoom==state.exitRoom){
        state.score += 100;
        logAction("You escaped the maze! +100 bonus!");
        state.running=false;
    }
}
// A single step: O(1) neighbor lookup (adjacency stored as set<int> per
// room), then O(E log V) worst case for the enemies' Dijkstra-step chase.
void doMove(char dir){
    if(!state.running) return;
    auto [x,y]=coordOf(state.currentRoom);
    int nx=x, ny=y;
    if(dir=='n') ny--; else if(dir=='s') ny++; else if(dir=='e') nx++; else if(dir=='w') nx--;
    if(nx<0||nx>=COLS||ny<0||ny>=ROWS){ logAction("There's a wall that way."); SfxBlocked(); return; }
    int target = idOf(nx,ny);
    if(!state.adj[state.currentRoom].count(target)){ logAction("There's a wall that way."); SfxBlocked(); return; }
    state.moveHistory.push(state.currentRoom);
    state.currentRoom = target;
    state.visitedRooms.insert(target);
    state.hintPath.clear();
    logAction("Moved to room " + to_string(target) + ".");
    SfxMove();
    moveEnemies();
    if(state.running) checkWin();
    if(!state.running){
        endBookkeeping();
        bool won = (state.currentRoom==state.exitRoom && state.health>0);
        if(won) SfxWin(); else SfxLose();
    }
}
// Undo = pop the move-history stack. O(1). Note this rewinds position only
// (matches the HTML edition); it intentionally doesn't refund HP lost or
// items picked up on that turn, so undo can't be used to farm score.
void doUndo(){
    if(!state.running) return;
    if(state.moveHistory.empty()){ logAction("Nothing to undo."); return; }
    int prev = state.moveHistory.top(); state.moveHistory.pop();
    state.currentRoom = prev;
    logAction("Undid move, back to room " + to_string(prev) + ".");
}
// O(1) map lookup for the current room's item, O(items) linear scan of the
// small fixed ITEM_POOL to find its score value.
void doPickup(){
    if(!state.running) return;
    auto it = state.roomItem.find(state.currentRoom);
    if(it==state.roomItem.end()){ logAction("Nothing to pick up here."); return; }
    string nm = it->second;
    state.inventory.push_back(nm);
    int val=0; for(auto& i: ITEM_POOL) if(i.name==nm) val=i.value;
    state.score += val;
    state.roomItem.erase(it);
    logAction("Picked up " + nm + " (+" + to_string(val) + ")");
    SfxPickup();
}
// BFS shortest path from current room to the exit. O(V+E) -- classic
// unweighted-graph shortest path, using a queue (see bfsPath()).
// A small score penalty is charged each use so hints are a genuine
// risk/reward trade-off instead of a free win.
void doHint(){
    if(!state.running) return;
    auto path = bfsPath(state.adj, state.currentRoom, state.exitRoom);
    if(path.empty()){ logAction("No path found."); return; }
    state.hintPath = path;
    state.hintsUsed++;
    const int penalty = 5;
    state.score = max(0, state.score - penalty);
    logAction("Hint: shortest path is " + to_string((int)path.size()-1) + " steps. (-" + to_string(penalty) + " score)");
}

// ---------------------------------------------------------------------------
// Win32 plumbing
// ---------------------------------------------------------------------------
#define ID_EDIT_NAME 101
#define ID_BTN_BEGIN 102
#define ID_BTN_N 103
#define ID_BTN_S 104
#define ID_BTN_E 105
#define ID_BTN_W 106
#define ID_BTN_PICKUP 107
#define ID_BTN_HINT 108
#define ID_BTN_UNDO 109
#define ID_BTN_NEWMAZE 110
#define ID_RADIO_EASY 111
#define ID_RADIO_MED 112
#define ID_RADIO_HARD 113
#define IDI_APPICON 500   // must match maze_escape_gui.rc

HWND hEditName, hBtnBegin, hBtnN, hBtnS, hBtnE, hBtnW, hBtnPickup, hBtnHint, hBtnUndo, hBtnNewMaze;
HWND hRadioEasy, hRadioMed, hRadioHard;
HFONT hFontUI, hFontTitle, hFontMono, hFontSmall;

void SetGameControlsVisible(bool visible){
    int sw = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(hBtnN, sw); ShowWindow(hBtnS, sw); ShowWindow(hBtnE, sw); ShowWindow(hBtnW, sw);
    ShowWindow(hBtnPickup, sw); ShowWindow(hBtnHint, sw); ShowWindow(hBtnUndo, sw); ShowWindow(hBtnNewMaze, sw);
}
void SetStartControlsVisible(bool visible){
    int sw = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(hEditName, sw); ShowWindow(hBtnBegin, sw);
}

// Where "New Maze" normally lives during active play (bottom of the control
// column) -- used to put it back after the game-over modal borrows it as
// the "Play Again" button.
// (New Maze button's normal in-game position is just CTRL_X, GRID_Y+182 --
// computed directly at the point of use below, since layout is now runtime.)

RECT g_overlayBox{0,0,0,0};
bool g_overlayActive = false;

// Keeps the start-screen / game-over modal box centered and moves whichever
// controls belong in it (name+begin+difficulty radios, or the reused
// New-Maze/"Play Again" button) there; restores normal layout once a game
// is actively running.
void SyncOverlayLayout(HWND hwnd){
    RECT rc; GetClientRect(hwnd, &rc);
    int boxH = !state.started ? OVERLAY_BOX_H_START : OVERLAY_BOX_H_END;
    int bx = (rc.right - OVERLAY_BOX_W)/2;
    int by = (rc.bottom - boxH)/2;
    g_overlayBox = RECT{bx,by,bx+OVERLAY_BOX_W,by+boxH};

    if(!state.started){
        g_overlayActive = true;
        SetGameControlsVisible(false);
        int rw = (OVERLAY_BOX_W-60)/3;
        MoveWindow(hRadioEasy, bx+30,          by+108, rw-6, 22, TRUE);
        MoveWindow(hRadioMed,  bx+30+rw,       by+108, rw-6, 22, TRUE);
        MoveWindow(hRadioHard, bx+30+2*rw,     by+108, rw-6, 22, TRUE);
        ShowWindow(hRadioEasy,SW_SHOW); ShowWindow(hRadioMed,SW_SHOW); ShowWindow(hRadioHard,SW_SHOW);
        MoveWindow(hEditName, bx+30, by+148, OVERLAY_BOX_W-60, 26, TRUE);
        MoveWindow(hBtnBegin, bx+30, by+188, OVERLAY_BOX_W-60, 30, TRUE);
        SetStartControlsVisible(true);
    } else if(!state.running){
        g_overlayActive = true;
        SetStartControlsVisible(false);
        ShowWindow(hRadioEasy,SW_HIDE); ShowWindow(hRadioMed,SW_HIDE); ShowWindow(hRadioHard,SW_HIDE);
        ShowWindow(hBtnN,SW_HIDE); ShowWindow(hBtnS,SW_HIDE); ShowWindow(hBtnE,SW_HIDE); ShowWindow(hBtnW,SW_HIDE);
        ShowWindow(hBtnPickup,SW_HIDE); ShowWindow(hBtnHint,SW_HIDE); ShowWindow(hBtnUndo,SW_HIDE);
        SetWindowTextA(hBtnNewMaze, "Play Again");
        MoveWindow(hBtnNewMaze, bx+(OVERLAY_BOX_W-140)/2, by+150, 140, 32, TRUE);
        ShowWindow(hBtnNewMaze, SW_SHOW);
    } else {
        g_overlayActive = false;
        SetStartControlsVisible(false);
        ShowWindow(hRadioEasy,SW_HIDE); ShowWindow(hRadioMed,SW_HIDE); ShowWindow(hRadioHard,SW_HIDE);
        SetWindowTextA(hBtnNewMaze, "New Maze");
        MoveWindow(hBtnNewMaze, CTRL_X, GRID_Y+182, CTRL_W, 28, TRUE);
        SetGameControlsVisible(true);
    }
}

// Dim the whole window and draw the bordered modal box (start / game-over
// screens), matching the HTML edition's .overlay + .overlay-box. AlphaBlend
// is the one place true transparency is needed, so it's used only here.
void DrawOverlay(HDC hdc, RECT client){
    if(!g_overlayActive) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
    HBRUSH blackBr = CreateSolidBrush(RGB(0,0,0));
    FillRect(memDC, &client, blackBr); DeleteObject(blackBr);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 190, 0};
    AlphaBlend(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, client.right, client.bottom, bf);
    SelectObject(memDC, oldBmp); DeleteObject(bmp); DeleteDC(memDC);

    RECT box = g_overlayBox;
    HBRUSH boxBr = CreateSolidBrush(COL_PANEL);
    HPEN boxPen = CreatePen(PS_SOLID, 1, COL_CURRENT);
    HGDIOBJ oldBr = SelectObject(hdc, boxBr);
    HGDIOBJ oldPen = SelectObject(hdc, boxPen);
    RoundRect(hdc, box.left, box.top, box.right, box.bottom, 14, 14);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
    DeleteObject(boxBr); DeleteObject(boxPen);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldF = (HFONT)SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, COL_TORCH);
    bool won = state.started && state.currentRoom==state.exitRoom && state.health>0;
    string title = !state.started ? "MAZE ESCAPE" : (won ? "YOU ESCAPED!" : "GAME OVER");
    RECT titleR{box.left, box.top+20, box.right, box.top+52};
    DrawTextA(hdc, title.c_str(), (int)title.size(), &titleR, DT_CENTER|DT_SINGLELINE);
    SelectObject(hdc, hFontUI);

    SetTextColor(hdc, COL_TEXTDIM);
    string sub = !state.started ? "Enter your name to descend into the dungeon"
                                 : ("Final score: " + to_string(state.score));
    RECT subR{box.left+24, box.top+58, box.right-24, box.top+90};
    DrawTextA(hdc, sub.c_str(), (int)sub.size(), &subR, DT_CENTER|DT_WORDBREAK);

    if(!state.started){
        SetTextColor(hdc, COL_TORCH);
        string diffLbl = "Choose difficulty:";
        RECT diffR{box.left+30, box.top+92, box.right-30, box.top+108};
        DrawTextA(hdc, diffLbl.c_str(), (int)diffLbl.size(), &diffR, DT_LEFT|DT_SINGLELINE);
    }
    SelectObject(hdc, oldF);
}

// Cheap "glow": paint a soft, slightly larger rounded rect in a dim shade of
// the glow color behind the real cell, instead of true per-pixel alpha
// blending -- much less code and nothing that can fail/crash, and against
// this near-black background it reads the same as a soft torchlight glow.
void DrawGlowBehind(HDC hdc, RECT r, COLORREF glowColor, int expand, int radius){
    RECT g{r.left-expand, r.top-expand, r.right+expand, r.bottom+expand};
    HBRUSH br = CreateSolidBrush(glowColor);
    HPEN pen = CreatePen(PS_SOLID, 1, glowColor);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, g.left, g.top, g.right, g.bottom, radius, radius);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
    DeleteObject(br); DeleteObject(pen);
}

void DrawGrid(HDC hdc){
    HFONT old = (HFONT)SelectObject(hdc, hFontMono);
    for(int y=0;y<ROWS;y++){
        for(int x=0;x<COLS;x++){
            int id = idOf(x,y);
            int cx = GRID_X + x*CELL, cy = GRID_Y + y*CELL;
            RECT r{cx,cy,cx+CELL-4,cy+CELL-4};

            COLORREF fill = COL_CELL;
            bool isHint = !state.hintPath.empty() && find(state.hintPath.begin(), state.hintPath.end(), id)!=state.hintPath.end();
            if(state.visitedRooms.count(id)) fill = COL_VISITED;
            if(isHint) fill = COL_HINT;
            bool hasEnemy=false;
            for(auto& e: state.enemies) if(e.room==id) hasEnemy=true;
            bool isDanger = hasEnemy && id!=state.currentRoom;
            if(isDanger) fill = COL_DANGER;
            bool isCurrent = (id==state.currentRoom);
            if(isCurrent) fill = COL_CURRENT;
            bool isExit = (id==state.exitRoom);
            // Enemy sharing your current room: the "@" glyph below normally
            // hides the "X" enemy marker, so give it its own red ring cue
            // instead of silently disappearing.
            bool enemyInCurrentRoom = isCurrent && hasEnemy;

            if(isCurrent) DrawGlowBehind(hdc, r, COL_CURRENT_GLOW, 5, 14);

            HBRUSH br = CreateSolidBrush(fill);
            HPEN pen = isExit ? CreatePen(PS_DASH, 1, COL_EXIT) : CreatePen(PS_SOLID, 1, COL_WALL);
            HBRUSH oldBr=(HBRUSH)SelectObject(hdc,br);
            HPEN oldPen=(HPEN)SelectObject(hdc,pen);
            RoundRect(hdc, r.left, r.top, r.right, r.bottom, 8, 8);
            SelectObject(hdc,oldBr); SelectObject(hdc,oldPen);
            DeleteObject(br); DeleteObject(pen);

            // inset accent ring: gold for the hint path, blood-red for
            // danger tiles AND for an enemy sharing your current room --
            // mirrors the HTML edition's inset box-shadow rings.
            if(isHint || isDanger || enemyInCurrentRoom){
                RECT inset{r.left+2, r.top+2, r.right-2, r.bottom-2};
                HPEN ringPen = CreatePen(PS_SOLID, 2, isHint?COL_EXIT:COL_DANGER);
                HGDIOBJ oldB = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                HGDIOBJ oldP = SelectObject(hdc, ringPen);
                RoundRect(hdc, inset.left, inset.top, inset.right, inset.bottom, 6, 6);
                SelectObject(hdc, oldB); SelectObject(hdc, oldP);
                DeleteObject(ringPen);
            }

            // walls: thick, clearly-visible line where the passage is
            // blocked (distinct color from the near-black cell outline).
            HPEN wallPen = CreatePen(PS_SOLID, 5, COL_WALLBLOCK);
            HPEN oldP2 = (HPEN)SelectObject(hdc, wallPen);
            if(x<COLS-1 && !state.adj[id].count(idOf(x+1,y))){
                MoveToEx(hdc, r.right, r.top, NULL); LineTo(hdc, r.right, r.bottom);
            }
            if(y<ROWS-1 && !state.adj[id].count(idOf(x,y+1))){
                MoveToEx(hdc, r.left, r.bottom, NULL); LineTo(hdc, r.right, r.bottom);
            }
            SelectObject(hdc, oldP2); DeleteObject(wallPen);

            // glyph
            string glyph;
            COLORREF tcol = COL_TEXT;
            if(id==state.exitRoom) glyph="EXIT";
            if(state.visitedRooms.count(id) && state.roomItem.count(id)){ glyph="$"; tcol=RGB(212,175,55); }
            if(hasEnemy && id!=state.currentRoom){ glyph="X"; tcol=RGB(255,255,255); }
            if(id==state.currentRoom){ glyph="@"; tcol=RGB(15,13,11); }
            if(!glyph.empty()){
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, tcol);
                DrawTextA(hdc, glyph.c_str(), (int)glyph.size(), &r, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            }
        }
    }
    SelectObject(hdc, old);
}

void DrawSidePanel(HDC hdc, RECT area){
    HBRUSH br = CreateSolidBrush(COL_PANEL);
    HPEN pen = CreatePen(PS_SOLID, 1, COL_WALL);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, area.left, area.top, area.right, area.bottom, 8, 8);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
    DeleteObject(br); DeleteObject(pen);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, hFontUI);

    int y = area.top + 10;
    auto putLine=[&](const string& s, COLORREF c, int fontSize=0){
        RECT r{area.left+10, y, area.right-10, y+22};
        SetTextColor(hdc, c);
        DrawTextA(hdc, s.c_str(), (int)s.size(), &r, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        y += 22;
    };

    putLine("INVENTORY", COL_TORCH);
    y+=2;
    if(state.inventory.empty()) putLine("(empty)", COL_TEXTDIM);
    else for(auto& it: state.inventory) putLine("- " + it, COL_TEXT);

    y += 14;
    putLine("ACTION LOG", COL_TORCH);
    y+=2;
    for(auto& m: state.logDeque) putLine(m, COL_TEXTDIM);

    y += 14;
    putLine("LEADERBOARD", COL_TORCH);
    y+=2;
    int shown=0;
    for(auto& e: leaderboard){
        if(shown>=5) break;
        string line = to_string(shown+1) + ". " + e.name + (e.won? " [WIN] " : " [DEAD] ") + to_string(e.score);
        putLine(line, COL_TEXT);
        shown++;
    }
    if(leaderboard.empty()) putLine("No runs yet.", COL_TEXTDIM);

    SelectObject(hdc, old);
}

void DrawHUD(HDC hdc){
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, COL_TORCH);
    RECT title{20,10,600,40};
    string t = "MAZE ESCAPE";
    DrawTextA(hdc, t.c_str(), (int)t.size(), &title, DT_LEFT|DT_SINGLELINE);
    SelectObject(hdc, hFontUI);

    ostringstream oss;
    oss << (state.playerName.empty()?"Player":state.playerName)
        << "   Room " << state.currentRoom << "/" << (N-1)
        << "   Score " << state.score
        << "   Hints " << state.hintsUsed;
    string status = oss.str();
    SetTextColor(hdc, COL_TEXT);
    RECT r2{20,44,700,66};
    DrawTextA(hdc, status.c_str(), (int)status.size(), &r2, DT_LEFT|DT_SINGLELINE);

    // HP bar -- red-to-orange gradient, like the HTML edition's hp-fill.
    int bx=20, by=68, bw=200, bh=14;
    HBRUSH bg = CreateSolidBrush(COL_PANEL2);
    RECT bgr{bx,by,bx+bw,by+bh};
    FillRect(hdc,&bgr,bg); DeleteObject(bg);
    int fillW = (int)(bw * (max(0,state.health)/100.0));
    if(fillW>0){
        TRIVERTEX vtx[2];
        vtx[0] = { bx, by, 0xA300, 0x3100, 0x2A00, 0x0000 }; // blood-500 #a3312a
        vtx[1] = { bx+fillW, by+bh, 0xE000, 0x8A00, 0x2E00, 0x0000 }; // torch-500 #e08a2e
        GRADIENT_RECT gr{0,1};
        GradientFill(hdc, vtx, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    }
    HPEN framePen = CreatePen(PS_SOLID, 1, COL_WALL);
    HGDIOBJ oldFP = SelectObject(hdc, framePen);
    HGDIOBJ oldFB = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, bgr.left, bgr.top, bgr.right, bgr.bottom, 4, 4);
    SelectObject(hdc, oldFP); SelectObject(hdc, oldFB); DeleteObject(framePen);

    string hpTxt = "HP " + to_string(max(0,state.health)) + "/100";
    RECT hpr{bx+bw+10, by-4, bx+bw+150, by+16};
    SetTextColor(hdc, COL_TEXT);
    DrawTextA(hdc, hpTxt.c_str(), (int)hpTxt.size(), &hpr, DT_LEFT|DT_SINGLELINE);

    SelectObject(hdc, old);

    // room description box
    RECT descR{20, GRID_Y + ROWS*CELL + 12, GRID_X+COLS*CELL-2, GRID_Y+ROWS*CELL+70};
    HBRUSH db = CreateSolidBrush(COL_PANEL2);
    FillRect(hdc,&descR,db); DeleteObject(db);
    RECT accent{descR.left, descR.top, descR.left+3, descR.bottom};
    HBRUSH ab = CreateSolidBrush(COL_TORCH);
    FillRect(hdc,&accent,ab); DeleteObject(ab);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc, COL_TEXT);
    HFONT oldf = (HFONT)SelectObject(hdc, hFontUI);
    string desc;
    if(!state.started) desc = "Enter your name and click Begin to start.";
    else if(!state.running){
        bool won = state.currentRoom==state.exitRoom && state.health>0;
        desc = won ? "YOU ESCAPED! Final score: " + to_string(state.score) + "  (click New Maze to play again)"
                   : "YOU DIED. Final score: " + to_string(state.score) + "  (click New Maze to try again)";
    } else {
        desc = describeRoom(state.currentRoom);
        if(state.roomItem.count(state.currentRoom)) desc += "  |  Item here: " + state.roomItem[state.currentRoom];
        for(auto& e: state.enemies) if(e.room==state.currentRoom) desc += "  |  ENEMY: " + e.name + "!";
    }
    RECT descInner{descR.left+8, descR.top+6, descR.right-8, descR.bottom-6};
    DrawTextA(hdc, desc.c_str(), (int)desc.size(), &descInner, DT_LEFT|DT_WORDBREAK);
    SelectObject(hdc, oldf);
}

void DrawLegend(HDC hdc){
    RECT area{CTRL_X, GRID_Y+182+40, CTRL_RIGHT, PANEL_BOTTOM};
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, hFontSmall);
    int y = area.top;
    auto line=[&](const string& sym, COLORREF symColor, const string& label){
        RECT symR{area.left, y, area.left+38, y+18};
        SetTextColor(hdc, symColor);
        DrawTextA(hdc, sym.c_str(), (int)sym.size(), &symR, DT_LEFT|DT_SINGLELINE);
        RECT lblR{area.left+40, y, area.right, y+18};
        SetTextColor(hdc, COL_TEXTDIM);
        DrawTextA(hdc, label.c_str(), (int)label.size(), &lblR, DT_LEFT|DT_SINGLELINE);
        y += 19;
    };
    line("EXIT", COL_TORCH, "= exit");
    line("$", RGB(212,175,55), "= item");
    line("X", RGB(255,255,255), "= enemy");
    line("@", COL_CURRENT, "= you");
    line("---", COL_EXIT, "= hint path");
    line("///", COL_WALLBLOCK, "= wall (blocked)");
    line("(ring)", COL_DANGER, "= enemy danger");
    SelectObject(hdc, old);
}

// Reads which difficulty radio is checked, applies it (ROWS/COLS/CELL +
// RecomputeLayout), resizes the window to the new CLIENT_W/CLIENT_H via
// AdjustWindowRectEx (same "derive the real size, don't guess" approach as
// the initial window creation), and moves the movement/action buttons to
// their recomputed positions. Called once, right before the first newGame().
void ApplyDifficultyAndResize(HWND hwnd){
    g_difficulty = 0;
    if(SendMessage(hRadioMed, BM_GETCHECK, 0, 0)==BST_CHECKED) g_difficulty = 1;
    else if(SendMessage(hRadioHard, BM_GETCHECK, 0, 0)==BST_CHECKED) g_difficulty = 2;

    const DifficultyPreset& p = DIFFICULTIES[g_difficulty];
    ROWS = p.rows; COLS = p.cols; CELL = p.cell;
    RecomputeLayout();

    RECT wr{0,0,CLIENT_W,CLIENT_H};
    AdjustWindowRectEx(&wr, (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE), FALSE, 0);
    SetWindowPos(hwnd, NULL, 0, 0, wr.right-wr.left, wr.bottom-wr.top, SWP_NOMOVE|SWP_NOZORDER);

    int gx = CTRL_X, gy = GRID_Y;
    MoveWindow(hBtnN, gx+40, gy,    40,32, TRUE);
    MoveWindow(hBtnW, gx,    gy+36, 40,32, TRUE);
    MoveWindow(hBtnS, gx+40, gy+36, 40,32, TRUE);
    MoveWindow(hBtnE, gx+80, gy+36, 40,32, TRUE);
    MoveWindow(hBtnPickup,  gx, gy+80,  CTRL_W,28, TRUE);
    MoveWindow(hBtnHint,    gx, gy+114, CTRL_W,28, TRUE);
    MoveWindow(hBtnUndo,    gx, gy+148, CTRL_W,28, TRUE);
    MoveWindow(hBtnNewMaze, gx, gy+182, CTRL_W,28, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CREATE:{
            hFontUI = CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
            hFontTitle = CreateFontA(26,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Georgia");
            hFontMono = CreateFontA(18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,"Consolas");
            hFontSmall = CreateFontA(13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");

            // Name/Begin only show before a game is running, and the grid is
            // only drawn once a game IS running -- so it's safe (and keeps
            // them clear of the side panel) to place them over the grid's
            // own column instead of drifting into the panel's x-range.
            hEditName = CreateWindowA("EDIT","Player",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
                GRID_X,GRID_Y,180,26,hwnd,(HMENU)ID_EDIT_NAME,NULL,NULL);
            SendMessage(hEditName, EM_LIMITTEXT, 16, 0); // matches HTML edition's maxlength="16"
            hBtnBegin = CreateWindowA("BUTTON","Begin",WS_CHILD|WS_VISIBLE,
                GRID_X+190,GRID_Y,90,26,hwnd,(HMENU)ID_BTN_BEGIN,NULL,NULL);

            // Difficulty picker (start screen only). WS_GROUP on the first
            // radio starts a new auto-radio group so exactly one of the
            // three is ever checked.
            hRadioEasy = CreateWindowA("BUTTON","Small (5x5)",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|WS_GROUP,
                0,0,10,10,hwnd,(HMENU)ID_RADIO_EASY,NULL,NULL);
            hRadioMed  = CreateWindowA("BUTTON","Medium (7x7)",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
                0,0,10,10,hwnd,(HMENU)ID_RADIO_MED,NULL,NULL);
            hRadioHard = CreateWindowA("BUTTON","Large (9x9)",WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
                0,0,10,10,hwnd,(HMENU)ID_RADIO_HARD,NULL,NULL);
            SendMessage(hRadioEasy, BM_SETCHECK, BST_CHECKED, 0);

            int gx = CTRL_X;
            int gy = GRID_Y;
            hBtnN = CreateWindowA("BUTTON","N",WS_CHILD|WS_VISIBLE,gx+40,gy,40,32,hwnd,(HMENU)ID_BTN_N,NULL,NULL);
            hBtnW = CreateWindowA("BUTTON","W",WS_CHILD|WS_VISIBLE,gx,gy+36,40,32,hwnd,(HMENU)ID_BTN_W,NULL,NULL);
            hBtnS = CreateWindowA("BUTTON","S",WS_CHILD|WS_VISIBLE,gx+40,gy+36,40,32,hwnd,(HMENU)ID_BTN_S,NULL,NULL);
            hBtnE = CreateWindowA("BUTTON","E",WS_CHILD|WS_VISIBLE,gx+80,gy+36,40,32,hwnd,(HMENU)ID_BTN_E,NULL,NULL);

            hBtnPickup = CreateWindowA("BUTTON","Pickup",WS_CHILD|WS_VISIBLE,gx,gy+80,CTRL_W,28,hwnd,(HMENU)ID_BTN_PICKUP,NULL,NULL);
            hBtnHint = CreateWindowA("BUTTON","Hint (BFS)",WS_CHILD|WS_VISIBLE,gx,gy+114,CTRL_W,28,hwnd,(HMENU)ID_BTN_HINT,NULL,NULL);
            hBtnUndo = CreateWindowA("BUTTON","Undo",WS_CHILD|WS_VISIBLE,gx,gy+148,CTRL_W,28,hwnd,(HMENU)ID_BTN_UNDO,NULL,NULL);
            hBtnNewMaze = CreateWindowA("BUTTON","New Maze",WS_CHILD|WS_VISIBLE,gx,gy+182,CTRL_W,28,hwnd,(HMENU)ID_BTN_NEWMAZE,NULL,NULL);

            for(HWND h: {hEditName,hBtnBegin,hBtnN,hBtnS,hBtnE,hBtnW,hBtnPickup,hBtnHint,hBtnUndo,hBtnNewMaze,
                         hRadioEasy,hRadioMed,hRadioHard})
                SendMessage(h, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            SyncOverlayLayout(hwnd);
            return 0;
        }
        case WM_COMMAND:{
            int id = LOWORD(wParam);
            if(id==ID_BTN_BEGIN){
                char buf[64]; GetWindowTextA(hEditName, buf, 64);
                string nm = buf; if(nm.empty()) nm="Player";
                ApplyDifficultyAndResize(hwnd);
                newGame(nm);
                SyncOverlayLayout(hwnd);
                InvalidateRect(hwnd,NULL,TRUE);
            } else if(id==ID_BTN_N){ doMove('n'); SyncOverlayLayout(hwnd); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_S){ doMove('s'); SyncOverlayLayout(hwnd); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_E){ doMove('e'); SyncOverlayLayout(hwnd); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_W){ doMove('w'); SyncOverlayLayout(hwnd); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_PICKUP){ doPickup(); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_HINT){ doHint(); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_UNDO){ doUndo(); InvalidateRect(hwnd,NULL,TRUE); }
            else if(id==ID_BTN_NEWMAZE){
                if(state.started){ newGame(state.playerName); SyncOverlayLayout(hwnd); InvalidateRect(hwnd,NULL,TRUE); }
            }
            return 0;
        }
        case WM_KEYDOWN:{
            if(!state.running) break;
            switch(wParam){
                case 'W': case VK_UP: doMove('n'); break;
                case 'S': case VK_DOWN: doMove('s'); break;
                case 'A': case VK_LEFT: doMove('w'); break;
                case 'D': case VK_RIGHT: doMove('e'); break;
                case 'P': doPickup(); break;
                case 'H': doHint(); break;
                case 'U': doUndo(); break;
            }
            SyncOverlayLayout(hwnd);
            InvalidateRect(hwnd,NULL,TRUE);
            return 0;
        }
        case WM_ERASEBKGND:{
            HDC hdc = (HDC)wParam;
            RECT r; GetClientRect(hwnd,&r);
            HBRUSH br = CreateSolidBrush(COL_BG);
            FillRect(hdc,&r,br); DeleteObject(br);
            return 1;
        }
        case WM_PAINT:{
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd,&ps);
            DrawHUD(hdc);
            if(state.started) DrawGrid(hdc);
            if(state.started) DrawLegend(hdc);
            RECT panelArea{ PANEL_X, PANEL_TOP, PANEL_X+PANEL_W, PANEL_BOTTOM };
            DrawSidePanel(hdc, panelArea);
            RECT client; GetClientRect(hwnd, &client);
            DrawOverlay(hdc, client);
            EndPaint(hwnd,&ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow){
    RecomputeLayout(); // default 5x5 layout; ApplyDifficultyAndResize() may resize later
    const char* CLASS_NAME = "MazeEscapeWindow";
    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    // Icon comes from the linked-in resource (maze_escape_gui.rc); falls
    // back to nothing drawn (Windows shows a generic icon) if the .exe was
    // built without the resource step -- never crashes either way.
    wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCE(IDI_APPICON));
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

    // Size the window from the CLIENT area we actually designed the layout
    // for (CLIENT_W x CLIENT_H), instead of guessing an outer window size
    // and hoping the title bar/borders leave enough room. This is what was
    // causing controls to end up outside the visible window before.
    RECT wr{0, 0, CLIENT_W, CLIENT_H};
    AdjustWindowRectEx(&wr, style, FALSE, 0);

    HWND hwnd = CreateWindowExA(0, CLASS_NAME, "Maze Escape",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg,NULL,0,0)){
        // WASD/Arrow/P/H/U are meant to control the game no matter which
        // child control (a button, etc.) currently has focus -- clicking a
        // button moves keyboard focus to that button, and Windows normally
        // routes WM_KEYDOWN to whichever window has focus, so the main
        // WndProc never saw the keypress. We force gameplay keys straight
        // to the main window here, EXCEPT while the name field is focused,
        // so typing a player name still works normally.
        if(msg.message==WM_KEYDOWN && GetFocus()!=hEditName){
            SendMessage(hwnd, WM_KEYDOWN, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
