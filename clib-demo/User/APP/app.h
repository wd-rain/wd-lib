#ifndef _APP_H_
#define _APP_H_

#include <stdint.h>

// Debug test result
#define APP_RB_TEST_RESULT_NOT_RUN 0U
#define APP_RB_TEST_RESULT_PASS    1U
#define APP_RB_TEST_RESULT_FAIL    2U

// Interface
extern volatile uint32_t app_rb_test_result;
extern volatile uint32_t app_rb_test_step;
extern volatile uint32_t app_rb_test_failed_step;
extern volatile uint32_t app_rb_test_count;
extern volatile uint32_t app_rb_test_size;
extern volatile uint32_t app_rb_test_free_size;
extern volatile uint32_t app_rb_test_capacity;
extern volatile uint32_t app_rb_test_is_empty;
extern volatile uint32_t app_rb_test_is_full;
extern uint8_t app_rb_test_storage[4U];
extern uint8_t app_rb_test_output[4U];

void app_init(void);
void app_task(void);

#endif
