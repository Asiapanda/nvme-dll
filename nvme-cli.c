#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvme-dll.h"


int main(int argc, char *argv[])
{
	int err = 0;
	if(argc < 2)
		return 0;
	if(strstr(argv[1], "list") != NULL)
	{
		err = GetDeviceList();
		if(err < 0)
		{
			fprintf(stderr, "GetDeviceList error:%d\n", err);
		}
		return 0;
	}
	else if(strstr(argv[1], "download") != NULL)
	{
		if(argc < 3)
			return 0;
		err = DownfwDevice(argv[2], argv[3]);
		if(err < 0)
			fprintf(stderr, "DownfwDevice error:%d\n", err);
		return 0;
	}
	else if(strstr(argv[1], "format") != NULL)
	{
		err = FormatDevice(argv[2]);
		if(err < 0)
			fprintf(stderr, "format error:%d\n", err);
		return 0;
	}
	else if(strstr(argv[1], "activate") != NULL)
	{
		if (argc < 5){
			printf("argc not is 5\n");
			return 0;
		}
		int slot = (argv[3][0]) - 48;
		int action = (argv[4][0]) - 48;
		err = ActivateDevice(argv[2], slot, action);
		if(err < 0)
			fprintf(stderr, "activate error:%d\n", err);
		return 0;
	}
	else {
		printf("args error\n");
	}
	return 0;
}
