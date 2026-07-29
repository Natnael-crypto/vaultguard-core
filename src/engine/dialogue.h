#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/config.h"
#include "engine/entity.h"
#include "engine/item.h"
#include "core/world.h"

#define MAX_DIALOGUE_NODES 64
#define MAX_DIALOGUE_TEXT 256
#define MAX_RESPONSES 8
#define MAX_RESPONSE_TEXT 128
#define MAX_QUEST_NAME 48
#define MAX_QUEST_DESC 256
#define MAX_QUESTS 24
#define MAX_QUEST_OBJECTIVES 4

typedef enum {
    QT_KILL_MONSTERS,
    QT_COLLECT_ITEMS,
    QT_REACH_DEPTH,
    QT_CLEAR_ROOM,
    QT_DELIVER_ITEM,
    QT_ESCORT_NPC
} QuestType;

typedef struct {
    uint32_t id;
    QuestType type;
    char name[MAX_QUEST_NAME];
    char description[MAX_QUEST_DESC];
    int target_id;
    int target_count;
    int current_count;
    int reward_gold;
    int reward_exp;
    Item reward_item;
    bool active;
    bool completed;
    bool turned_in;
    int turn_deadline;
} Quest;

typedef struct {
    int node_id;
    char text[MAX_DIALOGUE_TEXT];
    int response_count;
    char responses[MAX_RESPONSES][MAX_RESPONSE_TEXT];
    int next_nodes[MAX_RESPONSES];
    int quest_id;
    int shop_id;
    bool is_shop;
    bool is_quest_giver;
    bool is_trade;
} DialogueNode;

typedef struct {
    DialogueNode nodes[MAX_DIALOGUE_NODES];
    int node_count;
    int current_node;
    bool active;
    char npc_name[MAX_NAME_LEN];
} Dialogue;

void dialogue_init(Dialogue *d, const char *npc_name);
int dialogue_add_node(Dialogue *d, int node_id, const char *text);
int dialogue_add_response(Dialogue *d, int node_id, const char *response, int next_node);
void dialogue_start(Dialogue *d, int start_node);
int dialogue_choose(Dialogue *d, int choice);
const char *dialogue_get_text(const Dialogue *d);
int dialogue_get_response_count(const Dialogue *d);
const char *dialogue_get_response(const Dialogue *d, int index);
bool dialogue_is_active(const Dialogue *d);

void quest_init(Quest *q, uint32_t id, QuestType type, const char *name, const char *desc, int target, int count);
int quest_progress(Quest *q, int amount);
bool quest_is_complete(const Quest *q);
int quest_reward(Quest *q, Entity *player);
void quest_update_from_kill(Quest *q, int monster_id);
void quest_update_from_item(Quest *q, int item_id);
int quest_check_completion(Quest *q, const World *w);
const char *quest_type_name(QuestType type);
int quest_generate_random(Quest *q, uint32_t id, int player_level, RNGState *rng);

void dialogue_generate_greeting(Dialogue *d, const char *npc_name, bool has_quest, bool is_merchant);
void dialogue_generate_merchant(Dialogue *d, const char *npc_name);
void dialogue_generate_quest_giver(Dialogue *d, const char *npc_name, const Quest *q);

/* NPC-specific dialogue trees */
void dialogue_generate_blacksmith(Dialogue *d, const char *npc_name);
void dialogue_generate_wizard(Dialogue *d, const char *npc_name);
void dialogue_generate_priest(Dialogue *d, const char *npc_name);
void dialogue_generate_town_guard(Dialogue *d, const char *npc_name);
void dialogue_generate_farmer(Dialogue *d, const char *npc_name);

/* Quest template system */
int quest_get_template_count(void);
const char *quest_get_template_name(int idx);
int quest_generate_from_template(Quest *q, uint32_t id, int template_idx, int player_level, RNGState *rng);
int quest_generate_random_quest(Quest *q, uint32_t id, int player_level, RNGState *rng);
int quest_count_active(const Quest *quests, int total);
int quest_count_completed(const Quest *quests, int total);
int quest_get_next_id(const Quest *quests, int total);

#endif
