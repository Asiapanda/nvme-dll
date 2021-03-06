#ifdef __cplusplus
extern "C" {
#endif 

#ifndef __NVME__MAIN_H_
#define __NVME__MAIN_H_


/**************************************************
 *
 * 函数名称：GetDeviceList
 * 函数参数：无
 * 函数方法：获取NVMe驱动的设备列表
 *
 *************************************************/
int GetDeviceList();

/**************************************************
 *
 * 函数名称：DownfwDevice 
 * 函数参数：
 *		path: NVMe设备的路径，如果为NULL表示全部执行
 *		fwpath：firmware文件的路径
 * 函数方法：下载指定的fw到指定的设备上
 *
 *************************************************/
int DownfwDevice(char *path, char *fwpath); 

/**************************************************
 *
 * 函数名称：FormatDevice
 * 函数参数：
 *		path：NVMe设备的路径，如果为NULL表示全部执行
 * 函数方法：对指定的设备进行format
 *
 *************************************************/
int FormatDevice(char *path);

/**************************************************
 *
 * 函数名称：ActivateDevice
 * 函数参数：
 *		path：NVMe设备的路径
 *		slot：设备的槽位置，可通过帮助文档找到此值
 *		action：
 * 函数方法：对指定的设备进行激活
 *
 *************************************************/
int ActivateDevice(char *path, int slot, int action);

#endif

#ifdef __cplusplus
}
#endif
