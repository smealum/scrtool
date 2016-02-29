#include <3ds.h>
#include <stdio.h>

#include "scr.h"

typedef enum
{
	STATE_NONE,
	STATE_WORKING,
	STATE_DONE,
	STATE_OPEN_FAILED,
}state_t;

int main(int argc, char **argv)
{
	gfxInitDefault();

	consoleInit(GFX_BOTTOM, NULL);

	osSetSpeedupEnable(true);

	state_t state = STATE_NONE;
	if(scrInit()) state = STATE_OPEN_FAILED;
	bool combine;

	int total = 0;

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		printf("\x1b[0;0H");
		printf("----------------------\n");
		printf("|     scrtool 0.1    |\n");
		printf("----------------------\n");
		printf("                      \n");
		printf("                      \n");

		switch(state)
		{
			case STATE_NONE:
				{
					printf("  Press A to extract screenshots       \n");
					printf("  Press X to extract combined screens  \n");
					printf("  Press START to exit to menu          \n");
					printf("                                       \n");
					if(kDown & KEY_A)
					{
						state = STATE_WORKING;
						combine = false;
					}
					else if(kDown & KEY_X)
					{
						state = STATE_WORKING;
						combine = true;
					}
				}
				break;
			case STATE_WORKING:
				{
					printf("  Extracting screenshots...            \n");
					printf("  Hold B to cancel extraction          \n");
					printf("      %d                               \n", total);
					
					Result ret = combine ? scrCombinePop() : scrPop();
					
					if(ret) state = STATE_DONE;
					else total++;
					
					if(kDown & KEY_B) state = STATE_NONE;
				}
				break;
			case STATE_DONE:
				{
					printf("  Done !                               \n");
					printf("  Press START to exit to menu          \n");
					printf("                                       \n");
				}
				break;
			case STATE_OPEN_FAILED:
				{
					printf("  Could not open screenshot file !     \n");
					printf("  Press START to exit to menu          \n");
					printf("                                       \n");
				}
				break;

		}

		if (kDown & KEY_START) break;

		gspWaitForVBlank();
	}

	scrExit();

	// Exit services
	gfxExit();
	
	return 0;
}
