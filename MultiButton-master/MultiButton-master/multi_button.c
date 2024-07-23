/*
 * Copyright (c) 2016 Zibin Zheng <znbin@qq.com>
 * All rights reserved
 */

#include "multi_button.h"

#define EVENT_CB(ev)   if(handle->cb[ev])handle->cb[ev]((void*)handle) // 回调函数存在就调用
#define PRESS_REPEAT_MAX_NUM  15 /*!< The maximum value of the repeat counter */

//button handle list head.
static struct Button* head_handle = NULL;

static void button_handler(struct Button* handle);

/**
  * @brief  Initializes the button struct handle.
  * @param  handle: the button handle struct.
  * @param  pin_level: read the HAL GPIO of the connected button level.
  * @param  active_level: pressed GPIO level.
  * @param  button_id: the button id.
  * @retval None
  */
void button_init(struct Button* handle, uint8_t(*pin_level)(uint8_t), uint8_t active_level, uint8_t button_id)
{
	memset(handle, 0, sizeof(struct Button)); // 结构体归零
	handle->event = (uint8_t)NONE_PRESS; // 按钮事件绑定
	handle->hal_button_Level = pin_level; // 获取按钮电平状态
	//handle->button_level = handle->hal_button_Level(button_id); // 获取按钮当前电平
	handle->button_level = !active_level; // 按钮当前电平
	handle->active_level = active_level; // 按钮有效电平
	handle->button_id = button_id; // 按钮唯一标识符
}

/**
  * @brief  Attach the button event callback function.
  * @param  handle: the button handle struct.
  * @param  event: trigger event type.
  * @param  cb: callback function.
  * @retval None
  */
void button_attach(struct Button* handle, PressEvent event, BtnCallback cb)
{
	handle->cb[event] = cb; // 给按钮绑定回调函数
}

/**
  * @brief  Inquire the button event happen.
  * @param  handle: the button handle struct.
  * @retval button event.
  */
PressEvent get_button_event(struct Button* handle)
{
	return (PressEvent)(handle->event);
}

/**
  * @brief  Button driver core function, driver state machine.
  * @param  handle: the button handle struct.
  * @retval None
  */
static void button_handler(struct Button* handle)
{
	uint8_t read_gpio_level = handle->hal_button_Level(handle->button_id);

	//按钮按下过了 就会开始计数
	if((handle->state) > 0) handle->ticks++; 

	/*------------button debounce handle---------------*/ // 去逗
	if(read_gpio_level != handle->button_level) { //not equal to prev one
		//continue read 3 times same new level change
		if(++(handle->debounce_cnt) >= DEBOUNCE_TICKS) {
			handle->button_level = read_gpio_level; // 读了3次确实是电平修改了 那就修改按钮当前电平
			handle->debounce_cnt = 0;
		}
	} else { // 验证到 确实是 抖了一下 那就重置 下次还得验证3次
		handle->debounce_cnt = 0;
	}

	/*-----------------State machine-------------------*/
	switch (handle->state) {
	case 0:
		if(handle->button_level == handle->active_level) {	//检测到是按钮工作电平
			handle->event = (uint8_t)PRESS_DOWN;
			EVENT_CB(PRESS_DOWN);
			handle->ticks = 0;
			handle->repeat = 1; // 能进入case0 那一定是第一次按下才行
			handle->state = 1;
		} else {
			handle->event = (uint8_t)NONE_PRESS; // 设置为没有按下事件，松开状态，一直执行的
		}
		break;

	case 1:
		if(handle->button_level != handle->active_level) { //released press up
			handle->event = (uint8_t)PRESS_UP;
			EVENT_CB(PRESS_UP);
			handle->ticks = 0;
			handle->state = 2;
		} else if(handle->ticks > LONG_TICKS) { // 长按时间超过 设定值
			handle->event = (uint8_t)LONG_PRESS_START;
			EVENT_CB(LONG_PRESS_START);
			handle->state = 5;
		}
		break;

	case 2:
		if(handle->button_level == handle->active_level) { //press down again
			handle->event = (uint8_t)PRESS_DOWN;
			EVENT_CB(PRESS_DOWN);
			if(handle->repeat != PRESS_REPEAT_MAX_NUM) { // 只要不是重复按下了 15 次
				handle->repeat++; // 记录下 按下次数 已经是 2次 了
			}
			handle->event = (uint8_t)PRESS_REPEAT; // TODO 加上这一行代码
			EVENT_CB(PRESS_REPEAT); // repeat hit
			handle->ticks = 0;
			handle->state = 3;
		} else if(handle->ticks > SHORT_TICKS) { // 过了预定时间没有再次按下第2次
			if(handle->repeat == 1) {
				handle->event = (uint8_t)SINGLE_CLICK;
				EVENT_CB(SINGLE_CLICK); // 确认是 单击 事件 了
				// TODO handle->state = 0; //reset
			} else if(handle->repeat == 2) { // 处理双击事件
				handle->event = (uint8_t)DOUBLE_CLICK;
				EVENT_CB(DOUBLE_CLICK); // 确认 是 双击事件 了
				// 	TODO handle->state = 0; //reset
			}
			handle->state = 0;
		}
		break;

	case 3:
		// 没有到达 SHORT_TICKS值之前 松开了
		if(handle->button_level != handle->active_level) { //released press up again
			handle->event = (uint8_t)PRESS_UP;
			EVENT_CB(PRESS_UP);
			if(handle->ticks < SHORT_TICKS) { // 第二次 按下 到 松开的时间 正常的话
				handle->ticks = 0;
				handle->state = 2; //repeat press
			} else { // 第二次 按下按钮 长按
				handle->state = 0;
			}
		} else if(handle->ticks > SHORT_TICKS) { // 第二次按下是长按
			handle->state = 1; // 回去判断是否为长按
		}
		break;

	case 5: // 按下时间已经超过设定时间，已经处理长按事件
		if(handle->button_level == handle->active_level) { // 超过设定值还在 长按中
			//continue hold trigger
			handle->event = (uint8_t)LONG_PRESS_HOLD;
			EVENT_CB(LONG_PRESS_HOLD);
		} else { //长按之后的松开
			handle->event = (uint8_t)PRESS_UP;
			EVENT_CB(PRESS_UP);
			handle->state = 0; //reset
		}
		break;
	default:
		handle->state = 0; //reset
		break;
	}
}

/**
  * @brief  Start the button work, add the handle into work list.
  * @param  handle: target handle struct.
  * @retval 0: succeed. -1: already exist.
  */
int button_start(struct Button* handle)
{
	struct Button* target = head_handle;
	while(target) {
		if(target == handle) return -1;	//already exist.
		target = target->next;
	}
	handle->next = head_handle; // 头插法
	head_handle = handle;
	return 0;
}

/**
  * @brief  Stop the button work, remove the handle off work list.
  * @param  handle: target handle struct.
  * @retval None
  */
 // 注意 链表采用的是 头插法  (头)新2 -> 新1 -> 新0 -> NULL
void button_stop(struct Button* handle)
{
	struct Button** curr; // 新建一个指向按钮指针的指针
	for(curr = &head_handle; *curr; ) { // 将这个二级指针指向开头 for条件是不为NULL，还有的找
		struct Button* entry = *curr; // 新建一个 跟踪 按钮指针 负责跟踪二级指针指向的指针
		if(entry == handle) { // 发现跟踪指针 找到了 要停止的指针
			*curr = entry->next; // 如果是删除头节点 头节点也会相应的往后排一个
			//free(handle); // 所使用单片机 可能没有内存释放模块
			// TODO 是否需要修改 head_handle的地址 
			return;
		} else { // 没有找到，继续寻找下一个
			curr = &entry->next; // curr存放的是 上一个地址 -> next
		}
	}
}

/**
  * @brief  background ticks, timer repeat invoking interval 5ms.
  * @param  None.
  * @retval None
  */
 // 重复 5ms 运行一次
void button_ticks(void)
{
	struct Button* target;
	for(target=head_handle; target; target=target->next) {
		button_handler(target);
	}
}
