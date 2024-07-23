/*
 * Copyright (c) 2016 Zibin Zheng <znbin@qq.com>
 * All rights reserved
 */

#ifndef _MULTI_BUTTON_H_
#define _MULTI_BUTTON_H_

#include <stdint.h>
#include <string.h>

//According to your need to modify the constants.
#define TICKS_INTERVAL    5	//ms
#define DEBOUNCE_TICKS    3	//MAX 7 (0 ~ 7)
#define SHORT_TICKS       (300 /TICKS_INTERVAL)
#define LONG_TICKS        (1000 /TICKS_INTERVAL)


typedef void (*BtnCallback)(void*);

typedef enum {
    PRESS_DOWN = 0,         // 按钮被按下
    PRESS_UP,               // 按钮被释放
    PRESS_REPEAT,           // 按钮被重复按下（例如，长时间按住按钮）
    SINGLE_CLICK,           // 单次点击按钮
    DOUBLE_CLICK,           // 双击按钮
    LONG_PRESS_START,       // 按钮长按开始
    LONG_PRESS_HOLD,        // 按钮长时间保持按下状态
    number_of_event,        // 事件数量（用于循环或数组的大小）
    NONE_PRESS              // 没有按下事件（可能用于初始化或无效状态）
} PressEvent;


typedef struct Button {
    uint16_t ticks;                   // 定时器计数值，  按下的时间长度
    uint8_t  repeat : 4;             // 按钮重复计数，使用4位位域表示，可以存储0到15的值
    uint8_t  event : 4;              // 当前事件状态，使用4位位域表示
    uint8_t  state : 3;              // 按钮状态，使用3位位域表示 用户状态机的case
    uint8_t  debounce_cnt : 3;       // 防抖计数器，使用3位位域表示，帮助处理按钮抖动问题
    uint8_t  active_level : 1;       // 按钮的有效电平，1位表示按钮是高电平还是低电平有效
    uint8_t  button_level : 1;       // 当前按钮电平状态，1位表示按钮当前的电平（高或低）
    uint8_t  button_id;              // 按钮的唯一标识符，用于区分不同的按钮
    uint8_t  (*hal_button_Level)(uint8_t button_id_); // 函数指针，指向硬件抽象层（HAL）中的函数，用于获取按钮的电平状态
    BtnCallback  cb[number_of_event]; // 回调函数数组，用于处理不同按钮事件的回调
    struct Button* next;             // 指向下一个按钮节点的指针，用于形成链表
} Button;


#ifdef __cplusplus
extern "C" {
#endif

void button_init(struct Button* handle, uint8_t(*pin_level)(uint8_t), uint8_t active_level, uint8_t button_id);
void button_attach(struct Button* handle, PressEvent event, BtnCallback cb);
PressEvent get_button_event(struct Button* handle);
int  button_start(struct Button* handle);
void button_stop(struct Button* handle);
void button_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
