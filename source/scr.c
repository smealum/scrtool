#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "scr.h"
#include "lodepng.h"

#define SCR_CHUNK_SIZE 0x47000
#define SCR_CHUNK_ALIGNMENT 0x1000 // should really be SCR_CHUNK_SIZE but cant hurt to be flexible i guess
#define SCR_CHUNK_MAGIC 0x52435300
#define SCR_FILENAME "scr_"
#define SCR_PATH "sdmc:/screenshots"

static const char* scrTypes[] = {"TOP_LEFT", "TOP_RIGHT", "BOTTOM"};

FILE* scrFile = NULL;
int scrFileNo = -1;
int scrFileSize = 0;
u8* scrBuffer = NULL;
int scrId = -1;
bool scrIdType[3] = {true, true, true};

Result scrInit()
{
	scrFile = fopen("sdmc:/screenshots_raw.bin", "rb+");
	if(!scrFile) return -1;

	scrFileNo = fileno(scrFile);
	mkdir(SCR_PATH, 777);

	fseek(scrFile, 0, SEEK_END);
	scrFileSize = ftell(scrFile);

	scrBuffer = linearAlloc(0x46500);

	scrId = -1;

	return 0;
}

Result scrExit()
{
	if(scrFile) fclose(scrFile);

	if(scrBuffer) linearFree(scrBuffer);

	return 0;
}

Result scrGetInitialId(int* id)
{
	if(!id) return -1;
	
	DIR* dir = opendir(SCR_PATH);
	if(!dir) return -2;

	struct dirent* entry;

	*id = 0;

	while((entry = readdir(dir)))
	{
		char* name = entry->d_name;

		if(!memcmp(name, SCR_FILENAME, sizeof(SCR_FILENAME) - 1))
		{
			// prefix is right, read id
			int i = sizeof(SCR_FILENAME) - 1, cur_id = 0;
			unsigned char val;
			while((val = (name[i++] - '0')) && val < 10) cur_id = cur_id * 10 + val;
			if(cur_id > *id) *id = cur_id;
		}
	}

	closedir(dir);

	return 0;
}

Result scrGetId(scr_t type, int* id)
{
	if(!id) return -1;
	
	Result ret = 0;
	if(scrId < 0) ret = scrGetInitialId(&scrId);
	if(ret)return ret;

	if(scrIdType[type])
	{
		scrIdType[SCR_TOP_LEFT] = false;
		scrIdType[SCR_TOP_RIGHT] = false;
		scrIdType[SCR_BOTTOM] = false;
		scrId++;
	}

	scrIdType[type] = true;
	*id = scrId;

	return 0;
}

Result scrFindLast(int* offset)
{
	if(!offset) return -1;
	// not interested in anything after that point since if a chunk starts there it'll be incomplete anyway
	Result ret = fseek(scrFile, -SCR_CHUNK_SIZE + SCR_CHUNK_ALIGNMENT + 4, SEEK_END);
	if(ret) return -2;

	*offset = 0;

	u32 magic = 0;
	do
	{
		ret = fseek(scrFile, -SCR_CHUNK_ALIGNMENT - 4, SEEK_CUR);
		if(ret) return -3;
		fread(&magic, 4, 1, scrFile);
	} while(magic << 8 != SCR_CHUNK_MAGIC);

	ret = fseek(scrFile, -4, SEEK_CUR);
	if(ret) return -4;
	*offset = ftell(scrFile);

	return 0;
}

// buffer should be 0x46500, 0x4 aligned, on linear heap ideally
// offset < 0 means we're already there
Result scrExtract(int offset, u8* buffer, scr_t* _type)
{
	Result ret;
	if(offset >= 0)
	{
		ret = fseek(scrFile, offset, SEEK_SET);
		if(ret) return -1;
	}
	
	u32 magic = 0;
	fread(&magic, 4, 1, scrFile);
	if(magic << 8 != SCR_CHUNK_MAGIC)return -2;

	scr_t type = (magic >> 24) - '0';
	if(type > SCR_BOTTOM || type < SCR_TOP_LEFT)return -3;
	if(_type) *_type = type;

	GSP_FramebufferFormats format;
	int format_offset = (type == SCR_BOTTOM) ? 0x14 : 0x4;
	fseek(scrFile, format_offset, SEEK_CUR);
	fread(&format, 1, 1, scrFile);
	format &= 7;

	fseek(scrFile, 0x200 - 0x4 - 0x1 - format_offset, SEEK_CUR);

	int height = (type == SCR_BOTTOM) ? 320 : 400;
	int pixels = height * 240;
	
	u8* tmp = malloc(pixels * 3);
	if(!tmp) return -4;

	if(buffer)
	{
		switch(format)
		{
			case GSP_BGR8_OES:
				fread(tmp, pixels * 3, 1, scrFile);
				break;
			case GSP_RGB565_OES:
				{
					// i swear the offset makes sense
					fread(&tmp[pixels], pixels * 2, 1, scrFile);
					int i;
					for(i=0; i<pixels; i++)
					{
						u16 val = ((u16*)tmp)[i];
						tmp[i * 3 + 0] = (val & 0x1F) << 3;
						tmp[i * 3 + 1] = ((val >> 5) & 0x3F) << 2;
						tmp[i * 3 + 2] = ((val >> 11) & 0x1F) << 3;
					}
				}
				break;
			default:
				// never seen those used in the wild... will implement if they are
				break;
		}
	}

	int i, j;
	for(i=0; i<240; i++)
	{
		for(j=0; j<height; j++)
		{
			int offset_0 = (i + j * 240) * 3;
			int offset_1 = (j + (239 - i) * height) * 3;

			buffer[offset_1 + 2] = tmp[offset_0 + 0];
			buffer[offset_1 + 1] = tmp[offset_0 + 1];
			buffer[offset_1 + 0] = tmp[offset_0 + 2];
		}
	}

	free(tmp);

	return 0;
}

Result scrExport(scr_t type, u8* buffer)
{
	if(!buffer)return -1;

	int id = -2;

	scrGetId(type, &id);

	static char filename[256];
	sprintf(filename, SCR_PATH "/" SCR_FILENAME "%d_%s.png", id, scrTypes[type]);

	Result ret = lodepng_encode24_file(filename, buffer, (type == SCR_BOTTOM) ? 320 : 400, 240);
	if(ret) return ret;

	return 0;
}

Result scrPop()
{
	Result ret;
	int offset;
	scr_t type;

	ret = scrFindLast(&offset);
	if(ret) return ret;

	ret = scrExtract(offset, scrBuffer, &type);
	if(ret) return ret;

	ret = scrExport(type, scrBuffer);
	if(ret) return ret;

	ftruncate(scrFileNo, offset);

	return 0;
}
