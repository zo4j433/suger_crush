#include "hexboard.h"
#include "../global.h"
#include "../monster/monster.h"
#include "../player/player.h"
#include "../board/movable.h"          // 用來判斷可移動格

#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <math.h>
static ALLEGRO_FONT *debug_font = NULL;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


#define HEX_RADIUS 40  // 每個六邊形從中心到角的距離
#define HEX_WIDTH  (HEX_RADIUS * 2)
#define HEX_HEIGHT (sqrt(3) * HEX_RADIUS)
#define HEX_HORIZ_SPACING (HEX_WIDTH * 3.0 / 4.0)
#define HEX_VERT_SPACING  HEX_HEIGHT

// 棋盤常數
float HEX_SIZE = 30.0f; // 棋盤大小
const int rowLengths[ROWS]     = {8,9,10,11,10,11,10,11,10,9,8}; // 紀錄每層長度，共有 ROW(11) 層
const int rowOffsetTable[ROWS] = {2,1,1,0,1,0,1,0,1,1,2}; // 每層偏移格數

ALLEGRO_FONT *hex_font = NULL;

// 幾何工具
float get_hex_h(void){ return 2*HEX_SIZE;            }
float get_hex_w(void){ return sqrtf(3.f)*HEX_SIZE;   }
float get_hex_v(void){ return get_hex_h()*0.75f;     }

// 判斷周圍安全用
static const int dir_even[6][2]={{-1,-1},{-1,0},{0,-1},{0,1},{1,-1},{1,0}};
static const int dir_odd[6][2] ={{-1,0},{-1,1},{0,-1},{0,1},{1,0},{1,1}};

// 將整體畫面置中（給 draw_board() 和 get_hex_center() 共用）
static void get_board_origin(float *ox, float *oy){
    float W = get_hex_w();
    float V = get_hex_v();

    // 中心格 (5,5)
    int cRow = 5, cCol = 5;

    float x_center = rowOffsetTable[cRow] * W
                   + cCol * W
                   + (cRow % 2) * (W / 2);
    float y_center = cRow * V;

    *ox = WIDTH  / 2 - x_center;
    *oy = HEIGHT / 2 - y_center;
}

bool is_surrounding_safe(int r,int c){
    const int (*d)[2] = (r%2==0)? dir_even:dir_odd;
    for(int k=0;k<6;k++){
        int nr=r+d[k][0], nc=c+d[k][1];
        if(nr>=0&&nr<ROWS && nc>=0&&nc<rowLengths[nr] && is_monster_at(nr,nc)) // 沒有越界且有怪物 => 不安全
            return false;
    }
    return true;
}

// 初始化棋盤
void init_board(void){
    hex_font = al_create_builtin_font();
}

// 畫棋盤
#define HL_THICK 6

void draw_board(void)
{     if (!debug_font) {
        debug_font = al_create_builtin_font();
    }

    float W = get_hex_w(), V = get_hex_v();

    float ox, oy;
    get_board_origin(&ox, &oy);

    for(int r=0;r<ROWS;r++){
        float row_off = rowOffsetTable[r]*W;
        for(int c=0;c<rowLengths[r];c++){
            float cx = ox + row_off + c*W + (r%2)*(W/2);
            float cy = oy + r*V;

            // 六角形頂點
            float pts[12], a = M_PI/6;
            for(int i=0;i<6;i++){
                pts[2*i]   = cx + HEX_SIZE*cosf(a+i*M_PI/3);
                pts[2*i+1] = cy + HEX_SIZE*sinf(a+i*M_PI/3);
            }

            ALLEGRO_COLOR base = al_map_rgb(205,116,0);
            for(int i = 0; i < 6; i++){
                int j = (i + 1) % 6;
                al_draw_filled_triangle(
                    cx, cy,
                    pts[2*i],     pts[2*i+1],
                    pts[2*j],     pts[2*j+1],
                    base
                );
            }

            bool hl   = false;
            bool jump = false;

            if (!auto_moving && !monsters_animating()) {
                hl = is_movable(r, c);
                for (int i = 0; i < jump_cnt; i++) {
                    if (jump_tiles[i].r == r && jump_tiles[i].c == c) {
                        jump = true;
                        break;
                    }
                }
            }

            al_draw_polygon(pts,6,ALLEGRO_LINE_JOIN_ROUND,
                            al_map_rgb(230,230,230),2,0);
/*
                // debug: 顯示座標
            char buf[32];
            snprintf(buf, sizeof(buf), "(%d,%d)", r, c);
            al_draw_text(
                debug_font,
                al_map_rgb(255, 255, 255),
                cx,
                cy - 8,
                ALLEGRO_ALIGN_CENTER,
                buf
            );*/
            if(hl)
                al_draw_polygon(pts,6,ALLEGRO_LINE_JOIN_BEVEL,
                                al_map_rgb(255,255,0),HL_THICK,0);
            if(jump)
                al_draw_polygon(pts,6,ALLEGRO_LINE_JOIN_BEVEL,
                                al_map_rgb(0,255,180),HL_THICK,0);

            
        }
    }
}

void get_hex_center(int row, int col, float* center_x, float* center_y) {
    float W = get_hex_w();
    float V = get_hex_v();
    float row_off = rowOffsetTable[row] * W;

    float ox, oy;
    get_board_origin(&ox, &oy);

    *center_x = row_off + col * W + (row % 2) * (W / 2) + ox;
    *center_y = row * V + oy;
}