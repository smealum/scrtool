#include <3ds.h>
#include <stdio.h>

#include "scr.h"

int main(int argc, char **argv)
{
	gfxInitDefault();

	consoleInit(GFX_BOTTOM, NULL);

	scrInit();

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_A)
		{
			Result ret = scrPop();
			printf("popped : %d\n", (int)ret);
		}

		if (kDown & KEY_START) break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

	scrExit();

	// Exit services
	gfxExit();
	
	return 0;
}
