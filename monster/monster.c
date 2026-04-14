#include "monster.h"
#include <stdlib.h>
#include <stdio.h>
#include <allegro5/allegro_primitives.h>
#include "../board/hexboard.h"
#include "../ui/healthbar.h"
#include "../global.h"
#include "../board/movable.h"   //get_neighbor
#include "../ui/damagetext.h"
#include "../ui/scoretext.h"
#include "../ui/pebble.h"  
#include "../ui/sugar_dust.h"
#include "../player/player.h"
#include "../system/turn_system.h"
#include <allegro5/allegro_image.h>  
static ALLEGRO_BITMAP* monster_bitmaps[MON_TYPE_COUNT];
static bool player_in_ring(int mr, int mc, int range, int pr, int pc);
static void debug_print_attack_info(int index, int r, int c, int player_r, int player_c, int range)
{
    printf("\n[ATTACK DEBUG] idx=%d type=%d\n", index, monsters[index].type);
    printf("  current=(%d,%d) check=(%d,%d) next=(%d,%d)\n",
           monsters[index].r, monsters[index].c,
           r, c,
           monsters[index].next_r, monsters[index].next_c);
    printf("  player=(%d,%d) range=%d moving=%d delay=%d\n",
           player_r, player_c, range,
           monsters[index].moving, monsters[index].attack_delay);

    for (int d = 0; d < 6; d++) {
        int nr, nc;
        get_neighbor(r, c, d, &nr, &nc);
        printf("    dir %d -> (%d,%d)\n", d, nr, nc);
    }

    printf("  in_ring=%d\n", player_in_ring(r, c, range, player_r, player_c));
}
static bool is_tile_occupied(int r, int c, int self_index, int player_r, int player_c) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (i == self_index || !monsters[i].alive) continue;

        int occ_r = monsters[i].moving ? monsters[i].next_r : monsters[i].r;
        int occ_c = monsters[i].moving ? monsters[i].next_c : monsters[i].c;

        if (occ_r == r && occ_c == c)
            return true;
    }

    if (player_r == r && player_c == c)
        return true;

    return false;
}
static void get_screen_pos(int r, int c, float *out_x, float *out_y) {
    float HEX_W = get_hex_w(), HEX_V = get_hex_v();

    float x_center = rowOffsetTable[5]*HEX_W + 5*HEX_W + (5%2)*(HEX_W/2);
    float y_center = 5*HEX_V;
    float ox = WIDTH/2 - x_center;
    float oy = HEIGHT/2 - y_center;

    float row_off = rowOffsetTable[r]*HEX_W;
    *out_x = ox + row_off + c*HEX_W + (r%2)*(HEX_W/2);
    *out_y = oy + r*HEX_V;
}
/* ------------------------------------------------------------------
   檢查 (mr,mc) 怪物周圍是否有玩家
   － range = 1：只看第一圈 6 個鄰格
   － range = 2：再往外延伸一次 (共 18 格)
------------------------------------------------------------------ */
static bool player_in_ring(int mr,int mc,int range,int pr,int pc)
{
    int cur_ring[18][2];
    int cnt = 0;

    //先把第一圈 6 格收集進 cur_ring
    for (int d = 0; d < 6; d++) {
        int nr, nc;
        get_neighbor(mr, mc, d, &nr, &nc);
        cur_ring[cnt][0] = nr;
        cur_ring[cnt][1] = nc;
        cnt++;
    }

    //如果只要 range==1，直接比對即可
    if (range == 1) {
        for (int i = 0; i < cnt; ++i)
            if (cur_ring[i][0]==pr && cur_ring[i][1]==pc)
                return true;
        return false;
    }

    ///range==2：從第一圈每一格再展開一次
    int cnt2 = cnt;        
    for (int i = 0; i < 6; ++i) {
        int br = cur_ring[i][0];
        int bc = cur_ring[i][1];
        for (int d = 0; d < 6; ++d) {
            int nr, nc;
            get_neighbor(br, bc, d, &nr, &nc);

            //避免重複加入回到中心或第一圈本身
            if (nr==mr && nc==mc)               continue;
            bool dup = false;
            for (int k = 0; k < cnt2; ++k)
                if (cur_ring[k][0]==nr && cur_ring[k][1]==nc){
                    dup = true; break;
                }
            if (!dup) {
                cur_ring[cnt2][0]=nr; cur_ring[cnt2][1]=nc;
                cnt2++;
            }
        }
    }

    //檢查第二圈
    for (int i = 0; i < cnt2; ++i)
        if (cur_ring[i][0]==pr && cur_ring[i][1]==pc)
            return true;

    return false;
}


static inline int distance_manhattan(int r1, int c1, int r2, int c2) {
    return abs(r1 - r2) + abs(c1 - c2);
}


bool is_monster_at(int r, int c) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].r == r && monsters[i].c == c && monsters[i].alive) {
            return true;
        }
    }
    return false;
}

//列印當前所有存活怪物的座標與 HP
void debug_print_monsters(void)
{
    printf("⭑ Monster list this turn ⭑\n");
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].alive) {
            printf("  idx=%2d  (%d,%d)  HP=%d hitrange=%d\n",
                   i, monsters[i].r, monsters[i].c, monsters[i].hp,monsters[i].range);
        }
    }
    printf("-----------------------------\n");
}


Monster monsters[MAX_MONSTERS];
int monster_count = 0;

const MonsterStats MONSTER_STATS[MON_TYPE_COUNT] = {
    [MON_SHROOM]   = {"Mushroom", 80,  35, 1, 1},
    [MON_CABBAGE]  = {"Cabbage", 120, 45, 1, 1},
    [MON_TOFU]     = {"Tofu",     60,  35, 1, 2},
    [MON_DUMPLING] = {"Dumpling",250, 50, 2, 1},
    [MON_PRAWN]    = {"Prawn",   450, 55, 2, 2}
};


Monster create_monster(MonsterType type, int r, int c) {
    Monster m;
    m.type = type;
    m.r = r;
    m.c = c;
    m.next_r = r;
    m.next_c = c;
    m.max_hp = MONSTER_STATS[type].max_hp;
    m.hp = m.max_hp;
    m.atk = MONSTER_STATS[type].atk;
    m.move_range = MONSTER_STATS[type].move_range;
    m.range = MONSTER_STATS[type].atk_range;
    m.alive = true;
     get_screen_pos(r, c, &m.sx, &m.sy);
   m.tx = m.sx;  m.ty = m.sy;
  m.moving = false;
   m.speed = 4.0f;
   m.attack_delay = 0;
    return m;
}
//#define TEST_STRAWBERRY    1

void spawn_monsters_if_needed(int player_r, int player_c) {
#ifdef TEST_STRAWBERRY
    /* 測試模式：清場 + 固定位置重生 6 隻 Shroom ---------------- */
    static const Pos test_spots[4] = {
        {5,7},{4,8},{5,0},{4,5}
    };
    /* 1) 清掉所有舊怪 */
    for (int i=0;i<MAX_MONSTERS;i++){
        monsters[i].alive = false;
    }
    monster_count = 0;

    /* 2) 依序放 6 隻 Shroom */
    for (int i=0;i<4 && i<MAX_MONSTERS;i++){
        int slot = i;                       /* 每次用前 6 個 slot */
        monsters[slot] = create_monster(MON_SHROOM,
                                        test_spots[i].r,
                                        test_spots[i].c);
        monster_count++;
        printf("[TEST] Spawn Shroom at (%d,%d)\n",
               test_spots[i].r, test_spots[i].c);
    }
#else
    while (monster_count < MAX_MONSTERS) {
        //取得目前回合
        int turn = get_current_turn();

        //先算目前已有的 Dumpling 與 Prawn
        int dumplingCount = 0, prawnCount = 0;
        for (int k = 0; k < MAX_MONSTERS; k++) {
            if (monsters[k].alive) {
                if (monsters[k].type == MON_DUMPLING) dumplingCount++;
                if (monsters[k].type == MON_PRAWN)    prawnCount++;
            }
        }

        //嘗試在每個空位 spawn
        for (int i = 0; i < MAX_MONSTERS && monster_count < MAX_MONSTERS; i++) {
            if (!monsters[i].alive) {
                int r = rand() % ROWS;
                int c = rand() % rowLengths[r];
                if (r == player_r && c == player_c) continue;

                bool occupied = false;
                for (int j = 0; j < MAX_MONSTERS; j++) {
                    if (monsters[j].alive && monsters[j].r == r && monsters[j].c == c) {
                        occupied = true;
                        break;
                    }
                }
                if (occupied) continue;

                //隨機挑一個 type，但：
                //前20回合只挑 0..2（Shroom/Cabbage/Tofu）
                //之後允許 0..4，但 Dumpling/Prawn 最多各3隻
                MonsterType t;
                do {
                    t = rand() % MON_TYPE_COUNT;

                    // 若前20回合，強制只選前面三種
                    if (turn < 20 && (t == MON_DUMPLING || t == MON_PRAWN))
                        continue;

                    // 若超出上限，也重挑
                    if (t == MON_DUMPLING && dumplingCount >= 3)
                        continue;
                    if (t == MON_PRAWN    && prawnCount    >= 3)
                        continue;

                    // 合法就跳出迴圈
                    break;
                } while (1);

                //實際 spawn
                monsters[i] = create_monster(t, r, c);
                monster_count++;
                if (t == MON_DUMPLING) dumplingCount++;
                if (t == MON_PRAWN)    prawnCount++;

                printf("Spawned %s (HP: %d) at (%d, %d) on turn %d\n",
                       MONSTER_STATS[t].name,
                       MONSTER_STATS[t].max_hp,
                       r, c,
                       turn);
            }
        }
    }
    #endif
}



void draw_monsters() {
    float HEX_W = get_hex_w();

    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!monsters[i].alive) continue;

        float sx = monsters[i].sx;
        float sy = monsters[i].sy;
        ALLEGRO_BITMAP* bmp = monster_bitmaps[monsters[i].type];

        int w = al_get_bitmap_width(bmp);
        int h = al_get_bitmap_height(bmp);
        float display_w = HEX_W * 0.8f;
        float display_h = display_w * ((float)h / w);

        al_draw_scaled_bitmap(
            bmp,
            0, 0, w, h,
            sx - display_w / 2,
            sy - display_h / 2,
            display_w, display_h,
            0
        );

        draw_health_bar(monsters[i].sx - 20,
                        monsters[i].sy - 20,
                        40, 5,
                        monsters[i].hp,
                        monsters[i].max_hp);
    }
}


void monster_take_action(int index, int player_r, int player_c, int *player_hp)
{
    (void)player_hp;

    if (!monsters[index].alive) return;

    int r = monsters[index].r;
    int c = monsters[index].c;
    int range = monsters[index].range;
    int move_range = monsters[index].move_range;

    bool can_attack = (monsters[index].attack_delay <= 0);

    // 1) 先用舊位置判定攻擊
    if (can_attack && player_in_ring(r, c, range, player_r, player_c)) {
        debug_print_attack_info(index, r, c, player_r, player_c, range);
        printf(">>> ATTACK!\n");
        player.last_attacker_r = r;
        player.last_attacker_c = c;
        damage_player(monsters[index].atk);

        float sx = monsters[index].sx;
        float sy = monsters[index].sy;
        float tx, ty;
        get_screen_pos(player_r, player_c, &tx, &ty);
        spawn_pebble(sx, sy, tx, ty);

        monsters[index].attack_delay = 20;
    }

    // 2) 再決定要往哪裡走，但先不要改真正的 r,c
    int cur_r = r;
    int cur_c = c;

    for (int step = 0; step < move_range; step++) {
        int best_r = cur_r, best_c = cur_c;
        int best_dist = distance_manhattan(cur_r, cur_c, player_r, player_c);

        for (int d = 0; d < 6; d++) {
            int nr, nc;
            get_neighbor(cur_r, cur_c, d, &nr, &nc);

            if (nr < 0 || nr >= ROWS) continue;
            if (nc < 0 || nc >= rowLengths[nr]) continue;
            if (is_tile_occupied(nr, nc, index, player_r, player_c)) continue;

            int dist2 = distance_manhattan(nr, nc, player_r, player_c);
            if (dist2 < best_dist) {
                best_dist = dist2;
                best_r = nr;
                best_c = nc;
            }
        }

        if (best_r == cur_r && best_c == cur_c) break;

        cur_r = best_r;
        cur_c = best_c;
    }

    // 3) 只記錄目標位置，先不改 monsters[index].r/c
    monsters[index].next_r = cur_r;
    monsters[index].next_c = cur_c;

    if (cur_r != monsters[index].r || cur_c != monsters[index].c) {
        get_screen_pos(cur_r, cur_c, &monsters[index].tx, &monsters[index].ty);
        monsters[index].moving = true;
        apply_sugar_dust_to_monster(cur_r, cur_c);
    }
}
        

void damage_monster_at(int r, int c, int damage) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].alive && monsters[i].r == r && monsters[i].c == c) {
            monsters[i].hp -= damage;
            show_damage_text(r, c, damage, TARGET_MONSTER);
            printf("Jumped over monster at (%d, %d)! Dealt %d damage, remaining HP: %d\n", r, c, damage, monsters[i].hp);
            if (monsters[i].hp <= 0) {
                monsters[i].alive = false;
                printf("Monster at (%d, %d) has been defeated!\n", r, c);
                monster_count--;
                 // 經驗
               int xp_award = MONSTER_STATS[monsters[i].type].max_hp / 2;
                // 比如以怪物最大HP的一半當經驗值
                gain_xp(xp_award);
                show_damage_text(r, c, xp_award, TARGET_PLAYER);
                  int base = 0;
    switch (monsters[i].type) {
        case MON_SHROOM:   base = 100; break;
        case MON_CABBAGE:  base = 150; break;
        case MON_TOFU:     base = 100; break;
        case MON_DUMPLING: base = 200; break;
        case MON_PRAWN:    base = 300; break;
    }
    int extra = combo_hits - 4;
            float score_amp = (extra > 0) 
                ? (1.0f + extra * 0.02f) 
                : 1.0f;
            score += (int)(base* score_amp);
            show_score_text((int)(base* score_amp));

              printf("Player gained %d xp\n", xp_award);
            }
            return;
        }
    }
}


void update_monsters_animation(void) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &monsters[i];

        if (m->attack_delay > 0) {
            m->attack_delay--;
        }

        if (!m->alive || !m->moving) continue;

        float dx = m->tx - m->sx;
        float dy = m->ty - m->sy;
        float dist = sqrtf(dx*dx + dy*dy);

        if (dist <= m->speed) {
            m->sx = m->tx;
            m->sy = m->ty;
            m->moving = false;

            // 動畫走完，這時才真正更新邏輯座標
            m->r = m->next_r;
            m->c = m->next_c;
        } else {
            m->sx += dx / dist * m->speed;
            m->sy += dy / dist * m->speed;
        }
    }
}

void init_monster_graphics(void) {
    al_init_image_addon();
    const char* filenames[MON_TYPE_COUNT] = {
        "assets/image/monsters/shroom.png",
        "assets/image/monsters/cabbage.png",
        "assets/image/monsters/tofu.png",
        "assets/image/monsters/dumpling.png",
        "assets/image/monsters/prawn.png"
    };
    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        monster_bitmaps[i] = al_load_bitmap(filenames[i]);
        if (!monster_bitmaps[i]) {
            fprintf(stderr, "Failed to load %s\n", filenames[i]);
            exit(1);
        }
    }
}
void destroy_monster_graphics(void) {
    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        if (monster_bitmaps[i]) {
            al_destroy_bitmap(monster_bitmaps[i]);
            monster_bitmaps[i] = NULL;
        }
    }
    al_shutdown_image_addon();
}

bool monsters_animating(void) {
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (monsters[i].alive) {
            if (monsters[i].moving || monsters[i].attack_delay > 0) {
                return true;
            }
        }
    }
    return false;
}