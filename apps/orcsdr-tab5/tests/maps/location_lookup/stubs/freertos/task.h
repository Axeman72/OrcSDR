#pragma once
#include "FreeRTOS.h"
using TaskFunction_t=void(*)(void*);
inline constexpr int pdPASS=1;
inline constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
int xTaskCreatePinnedToCoreWithCaps(TaskFunction_t,const char*,unsigned,void*,unsigned,void*,int,unsigned);
void vTaskDelete(void*);
