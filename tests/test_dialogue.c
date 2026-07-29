#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "engine/dialogue.h"
#include "engine/entity.h"
#include "engine/item.h"
#include "core/world.h"
#include "systems/rng.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static void test_dialogue_init(void) {
    Dialogue d;
    dialogue_init(&d, "Test NPC");

    TEST("dialogue_init sets name");
    if (strcmp(d.npc_name, "Test NPC") == 0) PASS();
    else FAIL("wrong name");

    TEST("dialogue_init starts inactive");
    if (!d.active) PASS();
    else FAIL("should be inactive");
}

static void test_dialogue_nodes(void) {
    Dialogue d;
    dialogue_init(&d, "Shopkeeper");

    int nid = dialogue_add_node(&d, 0, "Hello, traveler!");
    TEST("dialogue_add_node creates node");
    if (nid >= 0 && d.node_count == 1) PASS();
    else FAIL("node not created");

    TEST("dialogue_get_text returns correct text");
    dialogue_start(&d, 0);
    if (strcmp(dialogue_get_text(&d), "Hello, traveler!") == 0) PASS();
    else FAIL("wrong text");
}

static void test_dialogue_responses(void) {
    Dialogue d;
    dialogue_init(&d, "Merchant");
    dialogue_add_node(&d, 0, "Welcome!");
    dialogue_add_node(&d, 1, "Goodbye!");

    int rc = dialogue_add_response(&d, 0, "Show me wares.", 1);
    TEST("dialogue_add_response succeeds");
    if (rc >= 0) PASS();
    else FAIL("response not added");

    TEST("dialogue_get_response_count");
    dialogue_start(&d, 0);
    if (dialogue_get_response_count(&d) == 1) PASS();
    else FAIL("wrong response count");

    TEST("dialogue_get_response returns text");
    if (strcmp(dialogue_get_response(&d, 0), "Show me wares.") == 0) PASS();
    else FAIL("wrong response text");
}

static void test_dialogue_navigation(void) {
    Dialogue d;
    dialogue_init(&d, "NPC");
    dialogue_add_node(&d, 0, "Start");
    dialogue_add_node(&d, 1, "Middle");
    dialogue_add_node(&d, 2, "End");
    dialogue_add_response(&d, 0, "-> Middle", 1);
    dialogue_add_response(&d, 1, "-> End", 2);

    dialogue_start(&d, 0);
    TEST("dialogue_choose moves to next node");
    int next = dialogue_choose(&d, 0);
    if (next == 1) PASS();
    else FAIL("wrong next node");

    TEST("dialogue_choose moves to end");
    next = dialogue_choose(&d, 0);
    if (next == 2) PASS();
    else FAIL("wrong end node");
}

static void test_quest_init(void) {
    Quest q;
    quest_init(&q, 1, QT_KILL_MONSTERS, "Slay Rats", "Kill 5 rats in the dungeon.", 0, 5);

    TEST("quest_init sets name");
    if (strcmp(q.name, "Slay Rats") == 0) PASS();
    else FAIL("wrong name");

    TEST("quest_init sets target count");
    if (q.target_count == 5) PASS();
    else FAIL("wrong target");

    TEST("quest_init starts active");
    if (q.active && !q.completed) PASS();
    else FAIL("wrong state");
}

static void test_quest_progression(void) {
    Quest q;
    quest_init(&q, 2, QT_KILL_MONSTERS, "Test Quest", "Test desc.", 0, 3);

    TEST("quest_progress increases count");
    quest_progress(&q, 1);
    if (q.current_count == 1 && !q.completed) PASS();
    else FAIL("wrong progress");

    quest_progress(&q, 2);
    TEST("quest completes at target");
    if (q.completed && quest_is_complete(&q)) PASS();
    else FAIL("quest not complete");
}

static void test_quest_random_gen(void) {
    RNGState rng;
    rng_init(&rng, 42);
    Quest q;
    int rc = quest_generate_random(&q, 5, 3, &rng);

    TEST("quest_generate_random succeeds");
    if (rc == 0) PASS();
    else FAIL("generation failed");

    TEST("generated quest has name");
    if (q.name[0] != '\0' && q.reward_gold > 0) PASS();
    else FAIL("empty name or no reward");
}

static void test_quest_reward(void) {
    Entity *player = entity_create("player", '@', 0, 0);
    player->gold = 0;
    player->experience = 0;

    Quest q;
    quest_init(&q, 3, QT_REACH_DEPTH, "Reach Depth", "Desc.", 5, 1);
    q.completed = true;
    q.reward_gold = 50;
    q.reward_exp = 100;

    int reward = quest_reward(&q, player);
    TEST("quest_reward gives gold");
    if (reward == 50 && player->gold == 50) PASS();
    else FAIL("wrong gold");

    TEST("quest_reward gives experience");
    if (player->experience == 100) PASS();
    else FAIL("wrong exp");

    entity_destroy(player);
    free(player);
}

static void test_dialogue_generate(void) {
    Dialogue d;
    Quest q;
    quest_init(&q, 4, QT_KILL_MONSTERS, "Quest", "Desc.", 0, 5);

    dialogue_generate_greeting(&d, "Test", true, true);
    TEST("dialogue_generate_greeting creates nodes");
    if (d.node_count > 0) PASS();
    else FAIL("no nodes");

    dialogue_generate_merchant(&d, "Merchant");
    TEST("dialogue_generate_merchant has merchant nodes");
    if (d.node_count > 0) PASS();
    else FAIL("no merchant nodes");

    dialogue_generate_quest_giver(&d, "Quest Giver", &q);
    TEST("dialogue_generate_quest_giver has quest nodes");
    if (d.node_count > 0) PASS();
    else FAIL("no quest nodes");
}

int main(void) {
    printf("=== Dialogue tests ===\n");
    test_dialogue_init();
    test_dialogue_nodes();
    test_dialogue_responses();
    test_dialogue_navigation();
    test_quest_init();
    test_quest_progression();
    test_quest_random_gen();
    test_quest_reward();
    test_dialogue_generate();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
