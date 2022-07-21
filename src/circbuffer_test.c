#include "circbuffer.c"

#include "ovtest.h"

static void test_create_destroy(void) {
  struct circbuffer *c = NULL;
  TEST_SUCCEEDED_F(circbuffer_create(&c));
  TEST_CHECK(c != NULL);
  TEST_SUCCEEDED_F(circbuffer_destroy(&c));
  TEST_CHECK(c == NULL);
}

static void test_write_read_mono(void) {
  static float const testdata[16] = {
      1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f};
  static float const *const input[1] = {testdata};
  float buffer[16] = {0};
  float *const output[1] = {buffer};
  struct circbuffer *c = NULL;

  TEST_SUCCEEDED_F(circbuffer_create(&c));
  TEST_CHECK(c != NULL);
  TEST_CHECK(c->ptr == NULL);
  TEST_CHECK(c->len == 0);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_set_channels(c, 1));
  TEST_CHECK(c->ptr != NULL);
  TEST_CHECK(c->len == 1);
  TEST_CHECK(c->ptr[0] == NULL);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  size_t written = 0;
  TEST_SUCCEEDED_F(circbuffer_read(c, output, 16, &written));
  TEST_CHECK(written == 0);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_write(c, input, 0));
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_write(c, input, 16));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 0);
  TEST_CHECK(memcmp(c->ptr[0], input[0], sizeof(float) * 16) == 0);

  TEST_SUCCEEDED_F(circbuffer_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output[0], testdata, sizeof(float) * 16) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_write(c, input, 16));
  TEST_SUCCEEDED_F(circbuffer_read(c, output, 8, &written));
  TEST_CHECK(written == 8);
  TEST_CHECK(memcmp(output[0], testdata, sizeof(float) * 8) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 8);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_write(c, input, 8));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 8);
  TEST_SUCCEEDED_F(circbuffer_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output[0], input[0] + 8, sizeof(float) * 8) == 0);
  TEST_CHECK(memcmp(output[0] + 8, input[0], sizeof(float) * 8) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 8);

  TEST_SUCCEEDED_F(circbuffer_write(c, input, 16));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 8);
  TEST_SUCCEEDED_F(circbuffer_write(c, input, 8));
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 24);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output[0], input[0], sizeof(float) * 16) == 0);
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 8);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_read(c, output, 16, &written));
  TEST_CHECK(written == 8);
  TEST_CHECK(memcmp(output[0], input[0], sizeof(float) * 8) == 0);
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_destroy(&c));
  TEST_CHECK(c == NULL);
}

TEST_LIST = {
    {"test_create_destroy", test_create_destroy},
    {"test_write_read_mono", test_write_read_mono},
    {NULL, NULL},
};
