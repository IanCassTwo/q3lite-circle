#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int key;       // Raw USB scancode or Q3 keycode
    int down;      // 1 = Pressed, 0 = Released
} rawInputEvent_t;

#define INPUT_QUEUE_SIZE 256

// Shared lock-free ring buffer variables
extern rawInputEvent_t g_inputQueue[INPUT_QUEUE_SIZE];
extern volatile int g_queueHead;
extern volatile int g_queueTail;

// Shared relative mouse movement accumulators
extern volatile int g_mouseDeltaX;
extern volatile int g_mouseDeltaY;

#ifdef __cplusplus
}
#endif
