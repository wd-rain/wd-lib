#include "app.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "main.h"
#include "os.h"
#include "tool/rb/rb.h"

#define APP_RB_TEST_STEP_INIT             1U
#define APP_RB_TEST_STEP_FULL_WRITE       2U
#define APP_RB_TEST_STEP_FULL_REJECT      3U
#define APP_RB_TEST_STEP_READ_HEAD        4U
#define APP_RB_TEST_STEP_WRAP_WRITE       5U
#define APP_RB_TEST_STEP_PEEK             6U
#define APP_RB_TEST_STEP_DROP             7U
#define APP_RB_TEST_STEP_READ_TAIL        8U
#define APP_RB_TEST_STEP_EMPTY_READ       9U
#define APP_RB_TEST_STEP_CLEAR            10U
#define APP_RB_TEST_STEP_DEINIT           11U

#define APP_RB_TEST_CHECK(expr, step) \
    do \
    { \
        app_rb_test_step = (step); \
        if (!(expr)) \
        { \
            app_rb_test_result = APP_RB_TEST_RESULT_FAIL; \
            app_rb_test_failed_step = (step); \
            return; \
        } \
    } while (0)

volatile uint32_t app_rb_test_result = APP_RB_TEST_RESULT_NOT_RUN;
volatile uint32_t app_rb_test_step = 0U;
volatile uint32_t app_rb_test_failed_step = 0U;
volatile uint32_t app_rb_test_count = 0U;
volatile uint32_t app_rb_test_size = 0U;
volatile uint32_t app_rb_test_free_size = 0U;
volatile uint32_t app_rb_test_capacity = 0U;
volatile uint32_t app_rb_test_is_empty = 0U;
volatile uint32_t app_rb_test_is_full = 0U;
uint8_t app_rb_test_storage[4U];
uint8_t app_rb_test_output[4U];

static void _app_rb_test_snapshot(const Rb *rb)
{
    app_rb_test_size = (uint32_t)rb_size(rb);
    app_rb_test_free_size = (uint32_t)rb_free_size(rb);
    app_rb_test_capacity = (uint32_t)rb_capacity(rb);
    app_rb_test_is_empty = (uint32_t)rb_is_empty(rb);
    app_rb_test_is_full = (uint32_t)rb_is_full(rb);
}

static void _app_rb_test_clear_output(void)
{
    size_t i;

    for (i = 0U; i < 4U; ++i)
    {
        app_rb_test_output[i] = 0U;
    }
}

static void _app_rb_test_run(void)
{
    Rb rb;
    uint8_t input1[4U] = {1U, 2U, 3U, 4U};
    uint8_t input2[3U] = {5U, 6U, 7U};
    uint8_t extra = 9U;

    app_rb_test_result = APP_RB_TEST_RESULT_FAIL;
    app_rb_test_failed_step = 0U;
    app_rb_test_count = 0U;
    _app_rb_test_clear_output();

    rb_init(&rb, app_rb_test_storage, 4U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_capacity == 4U, APP_RB_TEST_STEP_INIT);
    APP_RB_TEST_CHECK(app_rb_test_size == 0U, APP_RB_TEST_STEP_INIT);
    APP_RB_TEST_CHECK(app_rb_test_free_size == 4U, APP_RB_TEST_STEP_INIT);
    APP_RB_TEST_CHECK(app_rb_test_is_empty == 1U, APP_RB_TEST_STEP_INIT);
    APP_RB_TEST_CHECK(app_rb_test_is_full == 0U, APP_RB_TEST_STEP_INIT);

    app_rb_test_count = (uint32_t)rb_write(&rb, input1, 4U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 4U, APP_RB_TEST_STEP_FULL_WRITE);
    APP_RB_TEST_CHECK(app_rb_test_size == 4U, APP_RB_TEST_STEP_FULL_WRITE);
    APP_RB_TEST_CHECK(app_rb_test_free_size == 0U, APP_RB_TEST_STEP_FULL_WRITE);
    APP_RB_TEST_CHECK(app_rb_test_is_full == 1U, APP_RB_TEST_STEP_FULL_WRITE);

    app_rb_test_count = (uint32_t)rb_write(&rb, &extra, 1U);
    APP_RB_TEST_CHECK(app_rb_test_count == 0U, APP_RB_TEST_STEP_FULL_REJECT);

    _app_rb_test_clear_output();
    app_rb_test_count = (uint32_t)rb_read(&rb, app_rb_test_output, 2U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 2U, APP_RB_TEST_STEP_READ_HEAD);
    APP_RB_TEST_CHECK(app_rb_test_output[0] == 1U, APP_RB_TEST_STEP_READ_HEAD);
    APP_RB_TEST_CHECK(app_rb_test_output[1] == 2U, APP_RB_TEST_STEP_READ_HEAD);
    APP_RB_TEST_CHECK(app_rb_test_size == 2U, APP_RB_TEST_STEP_READ_HEAD);

    app_rb_test_count = (uint32_t)rb_write(&rb, input2, 3U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 2U, APP_RB_TEST_STEP_WRAP_WRITE);
    APP_RB_TEST_CHECK(app_rb_test_size == 4U, APP_RB_TEST_STEP_WRAP_WRITE);
    APP_RB_TEST_CHECK(app_rb_test_is_full == 1U, APP_RB_TEST_STEP_WRAP_WRITE);

    _app_rb_test_clear_output();
    app_rb_test_count = (uint32_t)rb_peek(&rb, app_rb_test_output, 4U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 4U, APP_RB_TEST_STEP_PEEK);
    APP_RB_TEST_CHECK(app_rb_test_output[0] == 3U, APP_RB_TEST_STEP_PEEK);
    APP_RB_TEST_CHECK(app_rb_test_output[1] == 4U, APP_RB_TEST_STEP_PEEK);
    APP_RB_TEST_CHECK(app_rb_test_output[2] == 5U, APP_RB_TEST_STEP_PEEK);
    APP_RB_TEST_CHECK(app_rb_test_output[3] == 6U, APP_RB_TEST_STEP_PEEK);
    APP_RB_TEST_CHECK(app_rb_test_size == 4U, APP_RB_TEST_STEP_PEEK);

    app_rb_test_count = (uint32_t)rb_drop(&rb, 1U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 1U, APP_RB_TEST_STEP_DROP);
    APP_RB_TEST_CHECK(app_rb_test_size == 3U, APP_RB_TEST_STEP_DROP);

    _app_rb_test_clear_output();
    app_rb_test_count = (uint32_t)rb_read(&rb, app_rb_test_output, 4U);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_count == 3U, APP_RB_TEST_STEP_READ_TAIL);
    APP_RB_TEST_CHECK(app_rb_test_output[0] == 4U, APP_RB_TEST_STEP_READ_TAIL);
    APP_RB_TEST_CHECK(app_rb_test_output[1] == 5U, APP_RB_TEST_STEP_READ_TAIL);
    APP_RB_TEST_CHECK(app_rb_test_output[2] == 6U, APP_RB_TEST_STEP_READ_TAIL);
    APP_RB_TEST_CHECK(app_rb_test_is_empty == 1U, APP_RB_TEST_STEP_READ_TAIL);

    app_rb_test_count = (uint32_t)rb_read(&rb, app_rb_test_output, 1U);
    APP_RB_TEST_CHECK(app_rb_test_count == 0U, APP_RB_TEST_STEP_EMPTY_READ);

    app_rb_test_count = (uint32_t)rb_write(&rb, input1, 2U);
    APP_RB_TEST_CHECK(app_rb_test_count == 2U, APP_RB_TEST_STEP_CLEAR);
    rb_clear(&rb);
    _app_rb_test_snapshot(&rb);
    APP_RB_TEST_CHECK(app_rb_test_is_empty == 1U, APP_RB_TEST_STEP_CLEAR);
    APP_RB_TEST_CHECK(app_rb_test_free_size == 4U, APP_RB_TEST_STEP_CLEAR);

    rb_deinit(&rb);
    app_rb_test_step = APP_RB_TEST_STEP_DEINIT;
    APP_RB_TEST_CHECK(rb.buffer == NULL, APP_RB_TEST_STEP_DEINIT);
    APP_RB_TEST_CHECK(rb.capacity == 0U, APP_RB_TEST_STEP_DEINIT);
    APP_RB_TEST_CHECK(rb.read_index == 0U, APP_RB_TEST_STEP_DEINIT);
    APP_RB_TEST_CHECK(rb.write_index == 0U, APP_RB_TEST_STEP_DEINIT);
    APP_RB_TEST_CHECK(rb.size == 0U, APP_RB_TEST_STEP_DEINIT);

    app_rb_test_result = APP_RB_TEST_RESULT_PASS;
}

void app_init(void)
{
    _app_rb_test_run();
}

void app_task(void)
{

}
