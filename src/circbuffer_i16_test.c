#include "circbuffer_i16.c"

#include "ovtest.h"

#include "inlines.h"

#ifdef __GNUC__
#  if __has_warning("-Wpadded")
#    pragma GCC diagnostic ignored "-Wpadded"
#  endif
#endif // __GNUC__

static void test_create_destroy(void) {
  struct circbuffer_i16 *c = NULL;
  TEST_SUCCEEDED_F(circbuffer_i16_create(&c));
  TEST_CHECK(c != NULL);
  TEST_SUCCEEDED_F(circbuffer_i16_destroy(&c));
  TEST_CHECK(c == NULL);
}

static void test_write_read_mono(void) {
  static int16_t const input[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  int16_t output[16] = {0};
  struct circbuffer_i16 *c = NULL;

  TEST_SUCCEEDED_F(circbuffer_i16_create(&c));
  TEST_CHECK(c != NULL);
  TEST_CHECK(c->ptr == NULL);
  TEST_CHECK(c->channels == 0);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_set_channels(c, 1));
  TEST_CHECK(c->ptr == NULL);
  TEST_CHECK(c->channels == 1);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  size_t written = 0;
  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 16, &written));
  TEST_CHECK(written == 0);
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 0));
  TEST_CHECK(c->buffer_size == 0);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 16));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 0);
  TEST_CHECK(memcmp(c->ptr, input, sizeof(int16_t) * 16) == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output, input, sizeof(int16_t) * 16) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 16));
  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 8, &written));
  TEST_CHECK(written == 8);
  TEST_CHECK(memcmp(output, input, sizeof(int16_t) * 8) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 8);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 8));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 8);
  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output, input + 8, sizeof(int16_t) * 8) == 0);
  TEST_CHECK(memcmp(output + 8, input, sizeof(int16_t) * 8) == 0);
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 8);

  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 16));
  TEST_CHECK(c->buffer_size == 16);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 8);
  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 8));
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 24);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 16, &written));
  TEST_CHECK(written == 16);
  TEST_CHECK(memcmp(output, input, sizeof(int16_t) * 16) == 0);
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 8);
  TEST_CHECK(c->writecur == 0);
  TEST_SUCCEEDED_F(circbuffer_i16_read(c, output, 16, &written));
  TEST_CHECK(written == 8);
  TEST_CHECK(memcmp(output, input, sizeof(int16_t) * 8) == 0);
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 0);
  TEST_CHECK(c->writecur == 0);

  TEST_SUCCEEDED_F(circbuffer_i16_write(c, input, 16));
  TEST_CHECK(c->buffer_size == 24);
  TEST_CHECK(c->remain == 16);
  TEST_CHECK(c->writecur == 16);
  static float const testdata[16] = {
      10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 110.f, 120.f, 130.f, 140.f, 150.f, 160.f};
  float const *const expected[1] = {testdata};
  float buffer[16] = {0};
  float *const fout[1] = {buffer};
  TEST_SUCCEEDED_F(circbuffer_i16_read_as_float(c, fout, 16, 10.f, &written));
  TEST_CHECK(written == 16);
  for (size_t i = 0; i < written; ++i) {
    TEST_CHECK(fcmp(fout[0][i], ==, expected[0][i], 1e-6f));
    TEST_MSG("expected %f", (double)expected[0][i]);
    TEST_MSG("got %f", (double)fout[0][i]);
  }

  TEST_SUCCEEDED_F(circbuffer_i16_destroy(&c));
  TEST_CHECK(c == NULL);
}

TEST_LIST = {
    {"test_create_destroy", test_create_destroy},
    {"test_write_read_mono", test_write_read_mono},
    {NULL, NULL},
};
