#include "gvssApp.h"
#include "main.h"
#include "gvssProtocol.h"
#include "remote_debug_msg_drv.h"
#include "log.h"
#include "gvss_type.h"

//#define BAUM_CAMERA_TYPE	
#define DATA_FROM_BAUME		1
#define DATA_FROM_SSZ		0
	
unsigned plcStatFlag = DATA_FROM_BAUME;
unsigned algSave = 0;
static GvssObj gGvss;

static int InitGvss(GvssObj *pObj)
{
	int ret = 0;

	plcStatFlag = DATA_FROM_BAUME;
	
	pObj->Com.port = PLCSERVER_PORT;
	ret = Gvss_ProtocolInit(&(pObj->Com));
	if(ret < 0)
	{
		ERR("Gvss_ProtocolInit fail\n");

		return -1;
	}
	
	return 0;
}

static void ExitGvss(GvssObj *pObj)
{
	Gvss_ProtocolExit(&(pObj->Com));

	log_exit();	
}


static unsigned int gGvssQuit = 0;
void Gvss_signalHandler(int signum)
{
    gGvssQuit = 1;

	printf("catch signal\n");
}

int gvss_dataFormat(Gvss_Result_Str *pResult,GvsstoPlc *info)
{
	unsigned int index;
	unsigned int t = 0;
	unsigned int size;
	
	Gvss_Result_BlockStr *recInfo;
	FinalResult *pFinalResult = (FinalResult *)((unsigned int)info + 320);
	
	info->flag[0] = GVSSTOPLC_FLAG;
	info->flag[1] = GVSSTOPLC_FLAG;
	info->flag[2] = GVSSTOPLC_FLAG;
	info->flag[3] = GVSSTOPLC_FLAG;
	
	pFinalResult->position = pResult->position;
	pFinalResult->distance = pResult->distance;

	info->size = sizeof(unsigned short)*2 + pResult->blockNum*sizeof(unsigned short)*3;
	size = sizeof(info->flag) + info->size + sizeof(info->size);
	
	info->data[t++] = pResult->blockNum;
	recInfo = pResult->block;
	for(index=0;index<pResult->blockNum;index++)
	{
		info->data[t++] = recInfo->area;
		recInfo++;
	}

	info->data[t++] = pResult->blockNum;
	recInfo = pResult->block;
	for(index=0;index<pResult->blockNum;index++)
	{
		info->data[t++] = recInfo->x;
		info->data[t++] = recInfo->y;
		recInfo++;
	}
	
	return size;
}

static char gRecvBuf[20] = {0};
void *GvsstskThrMain(void *pPrm)
{
	AlgObj *pObj = (AlgObj *)pPrm;
	int status;
	unsigned int isQuit = 0;
	AlgResultFrame *pFullFrame;
	Gvss_Result_Str *pResult;
	unsigned int size;	
	
	gGvssQuit = 0;
	if(InitGvss(&gGvss) < 0)
		return (void *)0;

	OSA_attachSignalHandler(SIGINT, Gvss_signalHandler);

	GvsstoPlc info;
	
	algSave = 0;
	while(gGvssQuit == 0)
	{
		status = gGvss.Com.wait_for_client(&(gGvss.Com));
		if(status < 0)
		{
			gGvssQuit = 1;
			isQuit = 1;
		}

		printf("accept\n");
		
		AppClient_On(TARGET_PLC,pObj);
		
		isQuit = gGvssQuit;
			
		while((isQuit == 0)&&Gvss_Get_connectState(&(gGvss.Com)))
		{
			memset(&info,0,sizeof(info));
			status = OSA_queGet(&pObj->ResultFullQue, (Int32 *) & pFullFrame,OSA_TIMEOUT_NONE);
			if((status != OSA_SOK)||(pFullFrame == NULL))
			{
				usleep(10000);
				continue;
			}
			pResult = (Gvss_Result_Str *)(pFullFrame->data);

			size = gvss_dataFormat(pResult,&info); 
			OSA_quePut(&pObj->ResultFreeQue,(Int32)(pFullFrame), OSA_TIMEOUT_FOREVER);
				
			if(Gvss_Get_connectState(&(gGvss.Com))&&(size <= GVSSTOPLC_SIZE))
			{
				status = gGvss.Com.send(&(gGvss.Com),&info,GVSSTOPLC_SIZE);				
				if(status < 0)
				{
					printf("send failed\n");
					isQuit = 1;
				}

				status = gGvss.Com.recv(&(gGvss.Com),gRecvBuf,20);
				if(status <= 0)
				{
					printf("recv failed\n");
					isQuit = 1;
				}
				else
				{
					if((gRecvBuf[0] != plcStatFlag) && (gRecvBuf[0] == DATA_FROM_BAUME))
						algSave = 1;
					plcStatFlag = gRecvBuf[0];
				}
				
			}	
			else
			{		
				isQuit = 1;
			}	
		}

		AppClient_Off(TARGET_PLC,pObj);

		Gvss_close_connect(&(gGvss.Com));
	}

	AppClient_Quit(pObj);
	
	ExitGvss(&gGvss);

	return (void *)0;	
}

