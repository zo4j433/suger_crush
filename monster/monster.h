#ifndef MONSTER_H
#define MONSTER_H

#include <stdbool.h>

#define MAX_MONSTERS 13
#define MON_TYPE_COUNT 5

typedef enum {
    MON_SHROOM,
    MON_CABBAGE,
    MON_TOFU,
    MON_DUMPLING,
    MON_PRAWN
} MonsterType;

typedef struct {
    const char* name;
    int max_hp;
    int atk;
    int move_range;
    int atk_range;
} MonsterStats;

typedef struct {
    int r, c;             
    int hp, max_hp;
    int atk;
    int range;
    int move_range;
    MonsterType type;
    bool alive;
    int next_r;
    int next_c;
    float sx, sy;          
    float tx, ty;          
    bool  moving;          
    float speed;           
    int  attack_delay; 
} Monster;

bool is_monster_at(int r, int c);

extern const MonsterStats MONSTER_STATS[MON_TYPE_COUNT];
extern Monster monsters[MAX_MONSTERS];
extern int monster_count;

Monster create_monster(MonsterType type, int r, int c);
void spawn_monsters_if_needed(int player_r, int player_c);
void monster_take_action(int index, int player_r, int player_c, int *player_hp);
void damage_monster_at(int r, int c, int damage);

void draw_monsters();
void update_monsters_animation();
void debug_print_monsters();  
void init_monster_graphics();
void destroy_monster_graphics();
bool monsters_animating(void);
#endif
