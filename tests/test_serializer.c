#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "io/serializer.h"
#include "core/state.h"

#define TEST(name) printf("  %-40s", #name);
#define PASS()     printf("PASS\n"); pass_count++;
#define FAIL(msg)  printf("FAIL: %s\n", msg); fail_count++;

static int pass_count = 0;
static int fail_count = 0;

static void test_serializer_int32(void) {
    FILE *f = fopen("test_temp.bin", "wb");
    TEST("serializer_write_int32 basic");
    if (f && serializer_write_int32(f, 42) == 0) PASS();
    else FAIL("write failed");
    if (f) fclose(f);

    f = fopen("test_temp.bin", "rb");
    int32_t val;
    TEST("serializer_read_int32 basic");
    if (f && serializer_read_int32(f, &val) == 0 && val == 42) PASS();
    else FAIL("read failed or mismatch");

    if (f) fclose(f);
    remove("test_temp.bin");
}

static void test_serializer_uint32(void) {
    FILE *f = fopen("test_temp.bin", "wb");
    TEST("serializer_write_uint32");
    if (f && serializer_write_uint32(f, 0xDEADBEEF) == 0) PASS();
    else FAIL("write failed");
    if (f) fclose(f);

    f = fopen("test_temp.bin", "rb");
    uint32_t val;
    TEST("serializer_read_uint32");
    if (f && serializer_read_uint32(f, &val) == 0 && val == 0xDEADBEEF) PASS();
    else FAIL("read failed or mismatch");
    if (f) fclose(f);
    remove("test_temp.bin");
}

static void test_serializer_string(void) {
    FILE *f = fopen("test_temp.bin", "wb");
    TEST("serializer_write_string");
    if (f && serializer_write_string(f, "hello") == 0) PASS();
    else FAIL("write failed");
    if (f) fclose(f);

    f = fopen("test_temp.bin", "rb");
    char buf[64];
    TEST("serializer_read_string");
    if (f && serializer_read_string(f, buf, 64) == 0 && strcmp(buf, "hello") == 0) PASS();
    else FAIL("read failed or mismatch");
    if (f) fclose(f);
    remove("test_temp.bin");
}

static void test_serializer_raw(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t readbuf[8] = {0};

    FILE *f = fopen("test_temp.bin", "wb");
    TEST("serializer_write_raw");
    if (f && serializer_write_raw(f, data, 5) == 0) PASS();
    else FAIL("write failed");
    if (f) fclose(f);

    f = fopen("test_temp.bin", "rb");
    TEST("serializer_read_raw");
    if (f && serializer_read_raw(f, readbuf, 5) == 0 &&
        readbuf[0] == 0x01 && readbuf[4] == 0x05) PASS();
    else FAIL("read failed or mismatch");
    if (f) fclose(f);
    remove("test_temp.bin");
}

static void test_serializer_int16(void) {
    FILE *f = fopen("test_temp.bin", "wb");
    TEST("serializer_write_int16");
    if (f && serializer_write_int16(f, -12345) == 0) PASS();
    else FAIL("write failed");
    if (f) fclose(f);

    f = fopen("test_temp.bin", "rb");
    int16_t val;
    TEST("serializer_read_int16");
    if (f && serializer_read_int16(f, &val) == 0 && val == -12345) PASS();
    else FAIL("read failed or mismatch");
    if (f) fclose(f);
    remove("test_temp.bin");
}

int main(void) {
    printf("=== Serializer tests ===\n");
    test_serializer_int32();
    test_serializer_uint32();
    test_serializer_string();
    test_serializer_raw();
    test_serializer_int16();
    printf("\n%d passed, %d failed\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
