#ifndef SCR_H
#define SCR_H

typedef enum
{
	SCR_TOP_LEFT = 0,
	SCR_TOP_RIGHT = 1,
	SCR_BOTTOM = 2,
}scr_t;

Result scrInit();
Result scrExit();
Result scrPop();

#endif
