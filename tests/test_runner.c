#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int total_passed = 0;
    int total_failed = 0;
    int rc;

    printf("========================================\n");
    printf("  Roguelike Terminal RPG - Test Suite\n");
    printf("========================================\n\n");

    /* Map tests */
    printf("Running map tests...\n");
    rc = system("\"test_map\" 2>&1");
    if (rc != 0) {
        printf("  Map tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Map tests: OK\n");
        total_passed++;
    }

    /* AI tests */
    printf("\nRunning AI tests...\n");
    rc = system("\"test_ai\" 2>&1");
    if (rc != 0) {
        printf("  AI tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  AI tests: OK\n");
        total_passed++;
    }

    /* Combat tests */
    printf("\nRunning combat tests...\n");
    rc = system("\"test_combat\" 2>&1");
    if (rc != 0) {
        printf("  Combat tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Combat tests: OK\n");
        total_passed++;
    }

    /* Serializer tests */
    printf("\nRunning serializer tests...\n");
    rc = system("\"test_serializer\" 2>&1");
    if (rc != 0) {
        printf("  Serializer tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Serializer tests: OK\n");
        total_passed++;
    }

    /* Ranged tests */
    printf("\nRunning ranged tests...\n");
    rc = system("\"test_ranged\" 2>&1");
    if (rc != 0) {
        printf("  Ranged tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Ranged tests: OK\n");
        total_passed++;
    }

    /* Crafting tests */
    printf("\nRunning crafting tests...\n");
    rc = system("\"test_crafting\" 2>&1");
    if (rc != 0) {
        printf("  Crafting tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Crafting tests: OK\n");
        total_passed++;
    }

    /* Dialogue tests */
    printf("\nRunning dialogue tests...\n");
    rc = system("\"test_dialogue\" 2>&1");
    if (rc != 0) {
        printf("  Dialogue tests returned %d\n", rc);
        total_failed++;
    } else {
        printf("  Dialogue tests: OK\n");
        total_passed++;
    }

    printf("\n========================================\n");
    printf("  Results: %d suites passed, %d suites failed\n", total_passed, total_failed);
    printf("========================================\n");

    return total_failed > 0 ? 1 : 0;
}
