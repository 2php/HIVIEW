#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "inc/gsf.h"
#include "mod/bsp/inc/bsp.h"
#include "mod/codec/inc/codec.h"

#include "gsf_rtsp_global.h"
#include "gsf_net_api.h"
#include "gsf_encode.h"
#include "base64.h"

struct rtsp_av_hdl_t {
  int vst;
  struct cfifo_ex* video;
  struct cfifo_ex* audio;
};


static pthread_mutex_t s_mutexRtsp = PTHREAD_MUTEX_INITIALIZER;
static GSF_RTSP_PLAY_CB stRtspPlayCb = {0};
static GSF_RTSP_CFG	 stRtspCfg 	  = {0};


//�û��Զ���ָ�� for test;
typedef struct // ��ȡ��������
{
  int nhandle;
  char userName;
}RTSP_USER_DATA_T;

// RETURN VALUE:  0: OK, -1: ERR;
//
int gsf_rtsp_get_usr_pwd_text(char *szUser /* in */, char *szPsw /*out*/)
{
  
	if(strcmp(szUser, "admin") == 0)
	{
	  strcpy(szPsw, "12345");
	  printf("[%s][%d]rtsp find user ok!! user [ %s ]\n", __FUNCTION__, __LINE__, szUser);
	  return 0;
	}
	
	printf("[%s][%d]rtsp find user err!! user [ %s ]\n", __FUNCTION__, __LINE__, szUser);
	return -1;
}



/*�޸�2017-11-14
*��������:��ȡ�û�������Ϣ
*����: type ��ȡ��������type:RTSP_CHECK_USER_GET_TEXT��������,RTSP_CHECK_USER_GET_DIGESTժҪ
RTSP_AUTH_USER_NULL������֤,����0(0)

*auth: RTSP_AUTH_GETPWD_T����
RTSP_AUTH_DIGEST_T ժҪ��Ϣ
* u:rtsp ÿ��rtsp session���û��Զ���ṹ��ָ��
*����ֵ:check�û�����ȷ����TRUE(1)���û��������ڷ���FALSE(0)
*/
int _gsf_rtsp_check_usr_pwd(int type , void *auth, void *u)
{

	int nRet = -1;

	if(auth == NULL)
		return -1;
	
	printf("[%s][%d]--debug--check usr pwd u(%p)\n",__FUNCTION__, __LINE__, u);
	if(u == NULL)
	{
		 printf("[%s][%d]--debug--check usr pwd u(%p) error\n", __FUNCTION__, __LINE__, u);
		//return -1;
	}
	
	if(type == RTSP_AUTH_USER_NULL)
	{
		return 0;
	}
	else if(type == RTSP_AUTH_USER_BASIC)		//��ȡ�û���������
	{

		RTSP_AUTH_BASIC_T *pAuthBasic = (RTSP_AUTH_BASIC_T *)auth;
		char szUserInfo[128] = {0};
		char szUser[64] = {0};
		char szPwd[64] = {0};
		char szDevPwd[64] = {0};


		if(NULL == pAuthBasic->basic)
		{
			return -1;
		}
		nRet = from64tobits(szUserInfo, pAuthBasic->basic);
		if(nRet <= 0)
			return -1;
		printf("base64(pAuthBasic->basic)--->(%s)\n", pAuthBasic->basic, szUserInfo);
		sscanf(szUserInfo,"%32[^:]:%32s", szUser, szPwd);
		printf("Auth basic user(%s),szpwd(%s)\n", szUser, szPwd);
		
		nRet = gsf_rtsp_get_usr_pwd_text(szUser, szDevPwd);
		if(nRet == 0)
		{
			if(strncmp(szDevPwd, szPwd, 32) == 0)
				return 0;
		}
		
		return -1;
	
	}
	else if(type == RTSP_AUTH_USER_DIGEST)
	{
		
		RTSP_AUTH_DIGEST_T *pAuthDig = (RTSP_AUTH_DIGEST_T *)auth;
		char szMd5Pwd[64] = {0};
		char szTextPwd[64] = {0};
		
		if(!(pAuthDig->realm) || !(pAuthDig->nonce) || !(pAuthDig->uri) || !(pAuthDig->userName) || !(pAuthDig->method) || !(pAuthDig->response))
		{
			return -1;
		}
		printf("[%s][%d]--debug-- realm(%s),nonce(%s),uri(%s),username(%s),method(%s)\n", __FUNCTION__, __LINE__, pAuthDig->realm, pAuthDig->nonce, pAuthDig->uri, pAuthDig->userName, pAuthDig->method);
		nRet = gsf_rtsp_get_usr_pwd_text(pAuthDig->userName, szTextPwd);
		if(nRet != 0)
		{
			printf("[%s][%d]--debug-- Digest Auth get text pwd error nRet(%d)\n", __FUNCTION__, __LINE__, nRet);
			return -1;
		}
		
		nRet = gsf_md5_auth_build_resonse(szMd5Pwd, sizeof(szMd5Pwd),
							0, pAuthDig->userName, pAuthDig->realm,
							szTextPwd, pAuthDig->nonce, NULL,
							NULL, NULL,
							pAuthDig->method, pAuthDig->uri);

		if(nRet < 0)
		{
			printf("[%s][%d]--debug--build md5pwd error \n", __FUNCTION__, __LINE__);
			return -1;
		}
		printf("[%s][%d]--debug--build md5pwd(%s),rsp(%s)\n", __FUNCTION__, __LINE__, szMd5Pwd, pAuthDig->response);
		if(strncmp(pAuthDig->response, szMd5Pwd, sizeof(szMd5Pwd)) == 0)
		{
			return 0;
		}
		return -1;
		
	}
	else
	{
		printf("[%s][%d]rtsp check user failed!! type error(%d)\n", __FUNCTION__, __LINE__, type);
		return -1;
	}
	
  	return -1;
}

unsigned int cfifo_recsize(unsigned char *p1, unsigned int n1, unsigned char *p2)
{
    unsigned int size = sizeof(gsf_frm_t);

    if(n1 >= size)
    {
        gsf_frm_t *rec = (gsf_frm_t*)p1;
        return  sizeof(gsf_frm_t) + rec->size;
    }
    else
    {
        gsf_frm_t rec;
        char *p = (char*)(&rec);
        memcpy(p, p1, n1);
        memcpy(p+n1, p2, size-n1);
        return  sizeof(gsf_frm_t) + rec.size;
    }
    
    return 0;
}

unsigned int cfifo_rectag(unsigned char *p1, unsigned int n1, unsigned char *p2)
{
    unsigned int size = sizeof(gsf_frm_t);

    if(n1 >= size)
    {
        gsf_frm_t *rec = (gsf_frm_t*)p1;
        return rec->flag & GSF_FRM_FLAG_IDR;
    }
    else
    {
        gsf_frm_t rec;
        char *p = (char*)(&rec);
        memcpy(p, p1, n1);
        memcpy(p+n1, p2, size-n1);
        return rec.flag & GSF_FRM_FLAG_IDR;
    }
    
    return 0;
}

unsigned int cfifo_recgut(unsigned char *p1, unsigned int n1, unsigned char *p2, void *u)
{
    unsigned int len = cfifo_recsize(p1, n1, p2);
    unsigned int l = CFIFO_MIN(len, n1);
    
    RTSP_AV_DATA *pData = (RTSP_AV_DATA*)u;
    
    if(len > pData->u32Size)
      return -2;
    
    char *p = (char*)pData->data;
    memcpy(p, p1, l);
    memcpy(p+l, p2, len-l);

    return len;
}




/*
�޸�2017-11-14:
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/
int _gsf_rtsp_open_alarm(RTSP_AV_HDL *hdl, int nCh, int nStreamNo, void *u)
{
  printf("[%s][%d]--debug--open alarm u(%p)\n", __FUNCTION__, u);
  return 0;
}

/*
�޸�2017-11-14:
hdl = NULL: get mediaInfo;
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/
int _gsf_rtsp_open_stream(RTSP_AV_HDL *hdl, int nCh, int nStreamNo, GSF_RTSP_MEDIA_INFO *pMediaInfo, void *u)
{
  printf("[%s][%d]--debug--open stream u(%p)\n", __FUNCTION__, __LINE__, u);
  if(nStreamNo < 0 || nStreamNo >= stRtspCfg.nStreamNum)
  {
    printf("%s => nStreamNo:%d err.\n", __func__, nStreamNo);
    return 0;
  }
  
  GSF_MSG_DEF(gsf_sdp_t, sdp, sizeof(gsf_msg_t)+sizeof(gsf_sdp_t));
  sdp->video_shmid = -1;
  sdp->audio_shmid = -1;
  int nRet = GSF_MSG_SENDTO(GSF_ID_CODEC_SDP, 0, GET, nStreamNo
                        , sizeof(gsf_sdp_t)
                        , GSF_IPC_CODEC
                        , 2000);
  if(nRet < 0)
  {
    printf("%s => get GSF_ID_CODEC_SDP err.\n" ,__func__);
    return 0;
  }
  
	if(pMediaInfo)
	{
    //audio;
    if(sdp->aenc.en)
    {
    	if (sdp->aenc.type == GSF_AENC_TYPE_G711A)
    	{
        	pMediaInfo->eAudioType = GSF_RTSP_AUDIO_G711A;
    	}
    	else if (sdp->aenc.type == GSF_AENC_TYPE_AACLC ||
    			     sdp->aenc.type == GSF_AENC_TYPE_EAAC  ||
    			     sdp->aenc.type == GSF_AENC_TYPE_EAACPLUS)
    	{
    		pMediaInfo->eAudioType = GSF_RTSP_AUDIO_AAC;
    	}
    	else
    	{
    		pMediaInfo->eAudioType = GSF_RTSP_AUDIO_G711U;
    	}
      pMediaInfo->u32AudioBit= 16;
      pMediaInfo->u32AudioSample = sdp->aenc.sprate;
    }
    else
    {
      pMediaInfo->eAudioType = GSF_RTSP_AUDIO_CODEC_END;
    }

    //video;
    if(sdp->venc.en)
    {
      if(sdp->venc.type == GSF_VENC_TYPE_H264)
      {
        pMediaInfo->eVideoType = GSF_RTSP_VIDEO_H264;
      }
      else if (sdp->venc.type == GSF_VENC_TYPE_MJPEG)
      {
          pMediaInfo->eVideoType = GSF_RTSP_VIDEO_MJPEG;
      }
      else if (sdp->venc.type == GSF_VENC_TYPE_H265)
      {
          pMediaInfo->eVideoType = GSF_RTSP_VIDEO_H265;
      }
      pMediaInfo->u16Width     = sdp->venc.width;   /* ��Ƶ���                         */
      pMediaInfo->u16Height    = sdp->venc.height;  /* ��Ƶ�߶�                         */
      pMediaInfo->u32Quality   = sdp->venc.qp;      /* ��Ƶ����                         */
      pMediaInfo->u32AvgBit    = sdp->venc.bitrate; /* �������ʣ���λ: λ/��            */
      pMediaInfo->u32Samples   = 90000;             /* �������ʣ���λ: HZ, �� 90000 HZ  */
      pMediaInfo->u32FrameRate = sdp->venc.fps;     /* ֡��, ��� 255 ֡/��             */
      
      pMediaInfo->xpsSize[0] = sdp->val[GSF_SDP_VAL_SPS].size<128?sdp->val[GSF_SDP_VAL_SPS].size:128;
      pMediaInfo->xpsSize[1] = sdp->val[GSF_SDP_VAL_PPS].size<128?sdp->val[GSF_SDP_VAL_PPS].size:128;
      pMediaInfo->xpsSize[2] = sdp->val[GSF_SDP_VAL_VPS].size<128?sdp->val[GSF_SDP_VAL_VPS].size:128;
      pMediaInfo->xpsSize[3] = sdp->val[GSF_SDP_VAL_SEI].size<128?sdp->val[GSF_SDP_VAL_SEI].size:128;
      memcpy(pMediaInfo->xpsData[0], sdp->val[GSF_SDP_VAL_SPS].data, pMediaInfo->xpsSize[0]);
      memcpy(pMediaInfo->xpsData[1], sdp->val[GSF_SDP_VAL_PPS].data, pMediaInfo->xpsSize[1]);
      memcpy(pMediaInfo->xpsData[2], sdp->val[GSF_SDP_VAL_VPS].data, pMediaInfo->xpsSize[2]);
      memcpy(pMediaInfo->xpsData[3], sdp->val[GSF_SDP_VAL_SEI].data, pMediaInfo->xpsSize[3]);
      printf("%s => fill media info: nStreamNo:%d, eVideoType:%d, u16Width:%d, u16Height:%d xps[%d,%d,%d,%d]\n"
            , __func__, nStreamNo, pMediaInfo->eVideoType, pMediaInfo->u16Width, pMediaInfo->u16Height
            , pMediaInfo->xpsSize[0], pMediaInfo->xpsSize[1], pMediaInfo->xpsSize[2], pMediaInfo->xpsSize[3]);
    }
    else
    {
      pMediaInfo->eVideoType = GSF_RTSP_VIDEO_END;
    }
  }
	
	if(hdl)
	{
	  printf("%s => gsf_stream_open nCh:%d, nStreamNo:%d\n", __FUNCTION__, nCh, nStreamNo);
	  
	  (*hdl) = (RTSP_AV_HDL)calloc(1, sizeof(struct rtsp_av_hdl_t));
	  (*hdl)->vst = nStreamNo;
	  if(sdp->video_shmid != -1)
	    (*hdl)->video = cfifo_shmat(cfifo_recsize, cfifo_rectag, sdp->video_shmid);
	  if(sdp->audio_shmid != -1)
	    (*hdl)->audio = cfifo_shmat(cfifo_recsize, cfifo_rectag, sdp->audio_shmid);
	}
  return 0;
}

int _gsf_rtsp_close_stream(RTSP_AV_HDL hdl)
{
  printf("%s => gsf_stream_close\n", __FUNCTION__);
  if(hdl)
  {
    if(hdl->video)
      cfifo_free(hdl->video);
    if(hdl->audio)
      cfifo_free(hdl->audio);
    free(hdl);
  }
  return 0;
}


int _gsf_rtsp_close_alarm(RTSP_AV_HDL hdl)
{
  printf("%s => gsf_stream_close\n", __FUNCTION__);
  return 0;
}

int _gsf_rtsp_venc_ctl(RTSP_AV_HDL hdl, RTSP_VENC_CTL_TYPE nType, void *data, int nSize, void *u)
{
  GSF_MSG_DEF(char, msgdata, sizeof(gsf_msg_t));
  return GSF_MSG_SENDTO(GSF_ID_CODEC_IDR, 0, SET, hdl->vst
                  , 0
                  , GSF_IPC_CODEC
                  , 2000);
}

int _gsf_rtsp_buffer_ctl(RTSP_AV_HDL hdl, GSF_RTSP_BUFFCTL_TYPE nType, void *u)
{
    cfifo_newest(hdl->video, 0);
    return 0;
}
/*
int gsf_printf_addr(unsigned char *pAddr, int nLen)
{
    int i = 0;
    for (i = 0; i < nLen; ++i)
    {
        printf("%02X ", pAddr[i]);
        if (((i + 1) % 16) == 0)
            printf("\n");
    }
    printf("\n");
    return 0;
}
*/

int _gsf_rtsp_get_video_data(RTSP_AV_HDL hdl, RTSP_AV_DATA *pData, void *u)
{  
    //printf("%s => pData->u32Size:%d\n", __func__, pData->u32Size);
    int ret = cfifo_get(hdl->video, cfifo_recgut, (unsigned char*)pData);
    if(ret > 0)
    {
  	  gsf_frm_t *rec = (gsf_frm_t*)pData->data;
      pData->u32Index = rec->seq;                         /* ���ݱ�ţ����: ֡��   */
      pData->u32Latest= rec->seq;                         /* ���µ�֡��             */
      pData->eType =                                      /* ���ݱ�������           */
            rec->video.encode == GSF_VENC_TYPE_H264?GSF_RTSP_VIDEO_H264:
            rec->video.encode == GSF_VENC_TYPE_MJPEG?GSF_RTSP_VIDEO_MJPEG:
            rec->video.encode == GSF_VENC_TYPE_H265?GSF_RTSP_VIDEO_H265:GSF_RTSP_VIDEO_END;
      memcpy(pData->u32NalLen, rec->video.nal, sizeof(rec->video.nal));/* ��ƵNALU��ַ */
      pData->u8IFrame = (rec->flag & GSF_FRM_FLAG_IDR);   // �Ƿ�ΪIFrame;
      pData->u8AudioChn = 0;                              //��Ƶ������1������ 2˫����
      pData->u8VideoRef = 0;                              //��Ƶ֡����ο�ֵ
      pData->u64TimeStamp = rec->pts;                     /* ���ݵ�ʱ��� */
      pData->u64TimeStamp *= 1000;
      pData->u16Width = rec->video.width;
      pData->u16Height = rec->video.height;
      pData->data += sizeof(gsf_frm_t);
  	  return rec->size;
    };
    return ret;
}

int _gsf_rtsp_get_audio_data(RTSP_AV_HDL hdl, RTSP_AV_DATA *pData, void *u)
{
    return 0;
}

int _gsf_rtsp_get_alarm_frame(RTSP_AV_HDL hdl, RTSP_AV_DATA *pData, void *u)
{
  return 0;
}

/*
�޸�2017-11-14:
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/
int _gsf_rtsp_notify(RTSP_MSG_NOTIFY eMsgNotify, int nCh, int nStreamType, char *pPar, void *u)
{
	printf("[%s][%d]--debug-- u(%p)\n", __FUNCTION__, __LINE__, u);
	
	switch (eMsgNotify)
	{
		case RTSP_MSG_PREVIEW_LINK:						
			break;

		case RTSP_MSG_PREVIEW_UNLINK:					
			break;
	}
	
	return 0;
}
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
static int os_get_ip_addr(const char *if_name, unsigned int *ip_addr)
{
    int fd;

    struct ifreq ifr;
    struct sockaddr_in *addr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0)
        {
            addr = (struct sockaddr_in *)&(ifr.ifr_addr);
            //inet_ntop(AF_INET, &addr->sin_addr, buffer, 20);
            *ip_addr = ntohl(*(unsigned int*)&addr->sin_addr);
            printf("os_getIpAddr : %X\n", *ip_addr);
        }
        else
        {
            close(fd);
            return(-1);
        }
    }
    else
    {
        perror("os_getIpAddr error :");
        return(-1);
    }
    
    close(fd);
    return(0);
}

/*
�޸�2017-11-14:
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/
int _gsf_rtsp_get_multicast_config(int st, RTSP_MULTICAST_CONFIG_S *mcast, void *u)
{
  unsigned int u32IpAddr;
  os_get_ip_addr("eth0", &u32IpAddr);
  
  // calc mcIP, mcPort;
  unsigned int mcIpCfg = (st == 0)?0xEC000000:
                    (st == 1)?0xEB000000:
                    0xEA000000;
                    
	unsigned int mcIp = (mcIpCfg & 0xFF000000) 
	               | (u32IpAddr & 0x00FFFFFF);
	
  unsigned short mcPort = (st == 0)?12340:
                    (st == 1)?23340:
                    33340;
  // calc stream mcPort;
	int i = 0;
	for(i = 0; i < RTSP_STREAM_BUTT; i++)
	{
		mcast->stPort[i] = mcPort + i*2;
		gsf_ip_n2a(mcIp, mcast->stIP[i], sizeof(mcast->stIP[i])-1);
		printf("get_multicast_config st:%d, i:%d, stIP:%s:%d\n"
				, st, i, mcast->stIP[i], mcast->stPort[i]);

	}
	mcast->ttl = 128;
	
	return 0;
}

/*
�޸�2017-11-14:
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/
int _gsf_rtsp_open_rec(RTSP_AV_HDL *hdl, int nCh, unsigned int startTime, unsigned int stopTime, GSF_RTSP_MEDIA_INFO *pMediaInfo, void *u)
{
  return 0;
}

int _gsf_rtsp_get_rec_frame(RTSP_AV_HDL handle, RTSP_AV_DATA *pData, int *frameType ,void *u)
{
	return 0;
}

int _gsf_rtsp_seek_frame(RTSP_AV_HDL handle, int playNpt, int seekTime, unsigned int u32FirtstStamp, unsigned int *pu32StampVideo, unsigned int *u32StampAudio)
{
	return 0;
}

int _gsf_rtsp_speed_play(RTSP_AV_HDL handle, int nSpeedValue)
{
	return 0;
}

int _gsf_rtsp_rec_pause(RTSP_AV_HDL handle, int nSpeedValue)
{
	return 0;
}

int _gsf_rtsp_rec_resume(RTSP_AV_HDL handle, int nSpeedValue)
{
	return 0;	
}

int _gsf_rtsp_rec_ctl(RTSP_AV_HDL pHdl, RTSP_REC_CTL_TYPE nType, void *data, int nSize, void *u)
{
  int ret = 0;
  
  switch(nType)
  {
    case RTSP_REC_CTL_SEEK:    // rtsp_rec_seek_t
      {
        rtsp_rec_seek_t *parm = (rtsp_rec_seek_t*)data;
        ret = _gsf_rtsp_seek_frame(pHdl, parm->playNpt, parm->seekTime, parm->u32FirtstStamp, parm->u32StampVideo, parm->u32StampAudio);
      }
      break;
    case RTSP_REC_CTL_SPEED:   // rtsp_rec_speed_t
      {
        rtsp_rec_speed_t *parm = (rtsp_rec_speed_t*)data;
        ret = _gsf_rtsp_speed_play(pHdl, parm->nSpeedValue);
      }
      break;
    case RTSP_REC_CTL_PAUSE:   // rtsp_rec_speed_t
      {
        rtsp_rec_speed_t *parm = (rtsp_rec_speed_t*)data;
        ret = _gsf_rtsp_rec_pause(pHdl, parm->nSpeedValue);
      }
      break;
    case RTSP_REC_CTL_RESUME:  // rtsp_rec_speed_t
      {
        rtsp_rec_speed_t *parm = (rtsp_rec_speed_t*)data;
        ret = _gsf_rtsp_rec_resume(pHdl, parm->nSpeedValue);
      }
      break;
  }
  return ret;
}


int _gsf_rtsp_close_rec(RTSP_AV_HDL handle)
{
	return 0;
}

/*
�޸�2017-11-14:
����*u :ÿ��rtsp session���û��Զ���ṹ��ָ�� (����ʶ�����ͬsession)
*/

//int _gsf_rtsp_get_rec_segment(int nCh, unsigned int unQueryStart, unsigned int unQueryEnd, unsigned int *pRealStart, unsigned int *pRealEnd)
int _gsf_rtsp_get_rec_segment(int nCh, unsigned int unQueryStart, unsigned int unQueryEnd, GSF_RTSP_REC_INFO *recInfo, void *u)
{
	return 0;	
}

int _gsf_rtsp_log(char *str)
{
  printf("%s", str);
	return 0;
}
//#define BACKCHANNEL_FILE_TEST 1
#if 0 //#ifdef BACKCHANNEL_FILE_TEST
/*
*������Ƶbackchannelд�ļ�/tmp/backchannel
*
*
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BACKCHANNEL_FILE_PATH "/tmp/backchannel"
static int g_handle = 0;

int test_backchannel_openfile()
{
	char sFilePath[128] =  BACKCHANNEL_FILE_PATH;
	int fd = 0;

	if(g_handle == 0)
	{
		//���Ѿ����ļ�
		fd = open(sFilePath, (O_CREAT|O_WRONLY|O_TRUNC));
		if(fd < 0)
		{
			printf("open file error, fd(%d)\n",fd);
			return -1;
		}

		g_handle = fd;
	}
	else
	{
		printf("open file error, file already g_handle(%d)\n",g_handle);
		return -1;
	}

	return fd;
}


int test_backchannel_wirtefile(int handle,void *pdata, int nDataLen)
{
	if(pdata == NULL || nDataLen <=0)
		return -1;

	int nRet = 0;
	if(handle != g_handle)
	{
		printf("wirte file error,handle(%), g_handle(%d)\n", handle, g_handle);
		return -1;
	}
	
	nRet = write(handle, pdata, nDataLen);
	if(nRet != nDataLen)
	{
		printf("write file data error nRet(%d), nDataLen(%d)", nRet, nDataLen);
	}
	
	return nRet;
}

int test_backchannel_closefile(int handle)
{
	int ret = 0;
	if(g_handle == handle)
	{	
		ret = close(handle);
		g_handle = 0;
		if(ret == 0)
		{
			printf("close file ok,handle(%d),\n", handle);
		}
		
	}
	else
	{
		printf("close file error,handle(%d),g_handle(%d)\n", handle, g_handle);
	}

	return 0;
}

int _gsf_rtsp_back_channel(RTSP_AV_HDL *pHdl, RTSP_BC_TYPE nType, RTSP_AV_DATA *pData, void *u)
{
	//printf("back channel debug NULL\n");
	//printf("pHdl(%d),nType(%d),pData(%p)\n",*pHdl,nType,pData);

	int nRet = 0;
	
	if(pHdl == NULL)
		return -1;

	if(nType == RTSP_BC_OPEN)
	{
		//*pHdl = rand();
		nRet = test_backchannel_openfile();
		if(nRet <= 0)
		{
			printf("callback open file error hdl(%d)\n",nRet);
			*pHdl = 0;
			return -1;
		}
		*pHdl = nRet;
		printf("callback OPEN hdl(%d)\n", *pHdl);
		return nRet;
	}
	else if(nType == RTSP_BC_RECV)
	{
		if(pData == NULL)
			return -1;
		
		printf("callback RECV hdl(%d) data[%p],size[%ld],Type[%d]\n", *pHdl, pData->data, pData->u32Size, pData->eType);
		nRet = test_backchannel_wirtefile(*pHdl, pData->data, pData->u32Size);
		if(nRet != pData->u32Size)
		{
			printf("callback write file error\n");
			return -1;
		}
		//printf("body[%s]\n",pData->data);
		
		
	}
	else if(nType == RTSP_BC_CLOSE)
	{
		printf("callback CLOSE hdl(%d) \n", *pHdl);
		nRet = test_backchannel_closefile(*pHdl);
		if(nRet != 0)
		{
			printf("callback CLOSE hdl(%d) error nRet(%d)\n", *pHdl, nRet);
		}
	}
	else
	{
		printf("RTSP_BC_TYPE error nType(%d)\n",nType);
	}
	return 0;
}
#endif

#if 1
/*****rtsp���� ������������backchannel����*************/
#include<sys/types.h>
#include<sys/socket.h>
#include <fcntl.h>
#include<errno.h>

#define GSF_BACKCHANNEL_LOCAL_IP "127.0.0.1"	//backchannel����ͨ��ip
#define GSF_BACKCHANNEL_PORT 3322				//ʹ��gsf_talk.c�ӿ�,ԭ����jxj�Խ�
#define GSF_BACKCHANNEL_MAGIC   0x33222233


#define GSF_BACKCHANNEL_TIMEOUT 500			//���������ӳ�ʱ,�ȴ�ʱ��ms

/*
typedef enum BACKCHANNEL_STATE_E {
	BACKCHANNEL_STATE_UNCONNECT_E = 0,
	BACKCHANNEL_STATE_SOCKET,			//����socket
	BACKCHANNEL_STATE_CONNECT_E,		//connect
	BACKCHANNEL_STATE_CLOSE,			//close
};
*/

typedef struct _R_GSF_TALK_AUDIO_ATTR_
{    
	unsigned char		u8AudioSamples;	    //������		0--8k 1--16k 2-32k
	unsigned char		u8EncodeType;			//��Ƶ�����ʽ0--pcm 1-g711a 2-g711u 3--g726
	unsigned char		u8AudioChannels;		//ͨ����		��ֻ֧��1
	unsigned char		u8AudioBits;			//λ��			16bit
}R_GSF_TALK_AUDIO_ATTR, *LPR_GSF_TALK_AUDIO_ATTR;


typedef struct _R_GSF_TALK_REQ_
{
    unsigned int   u32Magic;               //0x33222233 
    char            szUsr[64];
    char            szPsw[64];
    R_GSF_TALK_AUDIO_ATTR  stAttr;
    unsigned int   s32Res;                 //��Ӧ
}R_GSF_TALK_REQ, *LPR_GSF_TALK_REQ;

typedef struct _R_GSF_TALK_FRMAE_HDR_
{
	unsigned int		u32Magic;				//0x33222233  
	unsigned int		u32FrameNo; 			
	unsigned int		u32Pts;
	unsigned int		u32Len;
}R_GSF_TALK_FRMAE_HDR, *LPR_GSF_TALK_FRMAE_HDR;

//backchannel handle
typedef struct _R_GSF_BC_HANDLE_S_
{
	int nSocket;		//������������ͨ��socket

}R_GSF_BC_HANDLE_S,*LPR_GSF_BC_HANDLE_S;

typedef struct GSF_BACKCHANNEL_MAN_S_
{
	int nhandle;
	int state;

}GSF_BACKCHANNEL_MAN_S,*LPPGSF_BACKCHANNEL_MAN_S_;

static GSF_BACKCHANNEL_MAN_S gBackchannelM = {0};

/*
*����:gsf_rtsp_backchannel_connect
*����:rtsp���� �����������̷�������backchannel����
*����:
*����ֵ:
*/

int gsf_rtsp_backchannel_connect()
{
	int nret = 0;
	int error = 0;
	int error_len = sizeof(int);
	int n = 0;
	int nhandle = 0;
	int socketfd = 0;	
	struct sockaddr_in server_addr;
	int addrLen = sizeof(struct sockaddr_in);
	
	memset(&server_addr, 0, addrLen);
 	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = htonl(gsf_ip_a2n(GSF_BACKCHANNEL_LOCAL_IP));
	server_addr.sin_port        = htons(GSF_BACKCHANNEL_PORT);

	// 1
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd < 0)
	{
		printf("create socket error %d\n", errno);
	}

	printf("create socket ok fd(%d)\n",socketfd);

	// 2 ���÷���������ģʽ
	int flags = 0;
	flags = fcntl(socketfd, F_GETFL, 0);
	
	if (fcntl(socketfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return -1;
	}

	// 3
	if ((nret = connect(socketfd, (struct sockaddr *)&server_addr, (socklen_t)addrLen)) < 0)
	{
		if (errno != EINPROGRESS)	//���ӳ���
		{
			printf("connect fail\n");
			return -1;
		}
		//EINPROGRESS �ȴ���������
		fd_set w_set;				//
		FD_ZERO(&w_set);
		FD_SET(socketfd, &w_set);
		struct timeval tval;
		tval.tv_sec = 0;
		tval.tv_usec = GSF_BACKCHANNEL_TIMEOUT*1000;

		// 4
		if ((n = select(socketfd+1, NULL, &w_set, NULL, &tval)) == 0)
		{
			close(socketfd);
			errno = ETIMEDOUT;
			printf("backchannel TCP connect TIMEOUT\n");
			return -1;
		}
		
		if(FD_ISSET(socketfd, &w_set))
		{
			printf("select ok\n");		//select �ɶ�

			//��ȡ�׽��ֿ��ϵĴ���
			if(getsockopt(socketfd, SOL_SOCKET, SO_ERROR, (char *)&error, (socklen_t *)&error_len) < 0)
			{
				printf("getsockopt error (%d)\n", errno);
				close(socketfd);
				return -1;
			}

			//connect ok
			goto _connect_ok;
		}
		else
		{
			printf("select error,socketfd not set\n");
			return -1;
		}
	
	}
	else if (nret == 0)					//�Ѿ�������
	{
		printf("backchannel connect ok\n");
		goto _connect_ok;
		
	}
	_connect_ok:
	nhandle = socketfd;
	return nhandle;
	
	return nhandle;
}
/*
*tcp ��������������
*/
#define GSF_BACKCHANNEL_DEFAULT_SEND_TIMEOUT 2000	// 

static int gsf_rtsp_local_tcp_noblock_send(int hSock, unsigned char *pbuf,int size, int *pBlock, int timeOut)
{
	int  block = 0;	
	int  alllen = size;
	int  sended = 0;
	
	if (hSock < 0 || pbuf == NULL || size <= 0)
		return 0;

	if (pBlock != NULL)
		*pBlock = 0;

	while(alllen > 0)
	{	
		
		//sended = send(hSock,pbuf,alllen,gt | MSG_DONTWAIT);
		sended = send(hSock,pbuf,alllen,MSG_DONTWAIT);
		//sended = send(hSock,pbuf,alllen,0);
		printf("tcp send sended(%d),errno(%d)\n",sended,errno);
		if(0 == sended)
		{
			return GSF_RETURN_FAIL;
		}
		else if(sended < 1)
		{
			if(block > GSF_BACKCHANNEL_DEFAULT_SEND_TIMEOUT || timeOut == 0)
			{
				return GSF_RETURN_FAIL;
			}
			if(errno == EWOULDBLOCK || errno == EINTR || errno == EAGAIN)
			{	
				if(block++ < GSF_BACKCHANNEL_DEFAULT_SEND_TIMEOUT)
				{
					usleep(1000);		//
					continue;
				}
				else
				{
					if (pBlock != NULL) 
						*pBlock = 1;
					break;
				}
			}
			return GSF_RETURN_FAIL;
		}
		else
		{	
			if (pBlock != NULL) 
				*pBlock = 0;
			pbuf += sended;
			alllen -= sended;
		}
	}
		  
	if(alllen > 0)
		return GSF_RETURN_FAIL;
	return size;

}

static int gsf_rtsp_backchannel_start_req(int nSocket)
{
	int nret = 0; 
	R_GSF_TALK_REQ stReq = {0};
	R_GSF_TALK_REQ stRsp = {0};
	
	int reqLen = sizeof(R_GSF_TALK_REQ);

	if(nSocket < 0)
		return -1;
	#if 0
	if ((nret = gsf_select(&nSocket, 0x1, 0x1, 5000)) <= 0)
    {
        printf("backchannel start req select timeout\n");
        return -1;
    }
	#endif
	
	//send talk req
	memset(&stReq, 0, reqLen);
	stReq.u32Magic = GSF_BACKCHANNEL_MAGIC;

	//usr info null

	//audio attr
	stReq.stAttr.u8AudioSamples = 0;
	stReq.stAttr.u8EncodeType = 1;
	stReq.stAttr.u8AudioChannels = 1;
	stReq.stAttr.u8AudioBits = 16;
	nret =  gsf_rtsp_local_tcp_noblock_send(nSocket, (unsigned char*)&stReq, reqLen, NULL, 1000);
	if (nret != reqLen)
	{
		//close socket
		printf("backchannel start req send error,nret(%d),reqLen(%d),errno(%d)\n", nret, reqLen, errno);
		return -1;
	}
	printf("backchannel send req ok\n");
	//rsp
	memset(&stRsp, 0, reqLen);
	if ((nret = gsf_select(&nSocket, 0x1, 0x1, 5000)) <= 0)
    {
        printf("backchannel start req select timeout\n");
        return -1;
    }
	
	nret = gsf_tcp_noblock_recv(nSocket, (unsigned char *)&stRsp, reqLen, reqLen, 1000);
	if (nret != reqLen)
    {
        printf("backchannel start recv rsp error,nret(%d), reqLen(%d), errno(%d)\n", nret, reqLen,errno);
        return -1;
    }
	printf("backchannel recv req ok\n");
	if (stRsp.u32Magic != GSF_BACKCHANNEL_MAGIC || stRsp.s32Res != 0)
	{
		printf("backchannel recv rsp error,u32Magic(%x),res(%ld)\n", stRsp.u32Magic, stRsp.s32Res);
		return -1;
	}
	
	return 0;
}

/*
*����:gsf_rtsp_backchannel_senddata
*����:rtsp�����������̷���backchannel����
*����ֵ: -1����ʧ��,��ȷ�����ѷ������ݳ���pData->u32Size
*/
static int gsf_rtsp_backchannel_senddata(int nSocket, RTSP_AV_DATA *pData)
{
	int nRet = 0;
	int nSendLen = 0;
	char *buff = NULL;
	R_GSF_TALK_FRMAE_HDR stHdr = {0};
	int nHdrLen = sizeof(R_GSF_TALK_FRMAE_HDR);
	
	if (nSocket < 0 || pData == NULL || pData->u32Size < 0)
	{
		return -1;
	}

	memset(&stHdr, 0, nHdrLen);
	
	stHdr.u32Magic = GSF_BACKCHANNEL_MAGIC;
	stHdr.u32FrameNo = pData->u32Index;
	stHdr.u32Pts = (unsigned int)pData->u64TimeStamp / 1000;
	stHdr.u32Len = pData->u32Size;

	nSendLen = nHdrLen + pData->u32Size;
	buff = (char *)malloc(sizeof(char) * nSendLen);
	memset(buff, 0, sizeof(char) * nSendLen);
	if(buff == NULL)
	{
		printf("malloc error return\n");
		return -1;
	}
	memcpy(buff, &stHdr, nHdrLen);
	memcpy(buff+nHdrLen, pData->data, sizeof(char) * pData->u32Size);

	//nRet = gsf_rtsp_local_tcp_noblock_send(handle, (unsigned char*)buff, nSendLen, NULL, 1000);
	nRet = gsf_tcp_noblock_send(nSocket,(unsigned char*)buff,nSendLen, NULL, 1000);

	if(nRet != nSendLen)
	{
		printf("backchannel send to gsf len error,nSend(%d), nRet(%d)\n", nRet, nSendLen);
		nRet = -1;
	}
	
	free(buff);
	buff = NULL;

	return ((nRet>nHdrLen)?(nRet-nHdrLen):nRet);
}

/*
*����:gsf_rtsp_backchannel_closeconnect
*����:rtsp���̹ر�backchannel����
*����ֵ:-1�ر�socket����ʧ��;0�ر�socket��������
*/
int gsf_rtsp_backchannel_closeconnect(int nSocket)
{
	if(nSocket > 0)
	{
		close(nSocket);
		return 0;
	}
	
	return -1;
}

typedef enum _BACKCHANNEL_CB_ERR_E {
	BACKCHANNEL_CB_STATE_ERR_E = -6,		//�Ѿ������������̶Խ�����,����pHdl���Ѵ��ڵ�����handle����
	BACKCHANNEL_CB_CLOSE_ERR_E = -5,		//�ر��������̶Խ�����ʧ��
	BACKCHANNEL_CB_SEND_ERR_E = -4,			//�������̷���backchannel����ʧ��
	BACKCHANNEL_CB_STARTREQ_ERR_E = -3,		//�������̿�ʼ�Խ��������
	BACKCHANNEL_CB_CONNECT_ERR_E = -2,		//��������connect TCP ����ʧ��
	BACKCHANNEL_CB_PARAM_ERR_E = -1,		//���������������
	BACKCHANNEL_CB_OK_E = 0,				//����ɹ�

}BACKCHANNEL_CB_ERR_E;


//��back channel
static int gsf_rtsp_backchannel_open(RTSP_AV_HDL *pHdl)
{
	
	int nRet = 0;
	int nSocket = 0;
	R_GSF_BC_HANDLE_S *pBc = NULL;
	
	if(gBackchannelM.state == 0)
	{
		//connect tcp
		nSocket = gsf_rtsp_backchannel_connect();
		if(nSocket <= 0)
		{
			printf("callback tcp connect gsf error socket(%d)\n",nSocket);
			nRet = BACKCHANNEL_CB_CONNECT_ERR_E;
			goto _done_handle;
		}
		//printf("callback connect hdl(%d)\n", nRet);
		//start req talk
		if(gsf_rtsp_backchannel_start_req(nSocket) != 0)
		{
			printf("callback tcp req or rsq error,then close socket\n");
			close(nSocket);
			nRet = BACKCHANNEL_CB_STARTREQ_ERR_E;
			goto _done_handle;
		}
		
		pBc = (R_GSF_BC_HANDLE_S *)malloc(sizeof(R_GSF_BC_HANDLE_S));
		if(pBc == NULL)
		{
			close(nSocket);
			nRet = BACKCHANNEL_CB_PARAM_ERR_E;
			goto _done_handle;
		}
		
		memset(pBc,0,sizeof(R_GSF_BC_HANDLE_S));
		pBc->nSocket = nSocket;
		gBackchannelM.state = 1;
		printf("callback connect->req->rsp ok socket(%d)\n", nSocket);
		nRet = BACKCHANNEL_CB_OK_E;
		//OPEN OK return handle 
		goto _done_handle;
		
	}
	else
	{
		printf("connect error,busy \n");
		
		nRet = BACKCHANNEL_CB_STATE_ERR_E;
		goto _done_handle;
	}

	//�ص�����
	_done_handle:
	*pHdl = (RTSP_AV_HDL)pBc;
	return nRet;
}

//send 
static int gsf_rtsp_backchannel_send(RTSP_AV_HDL Hdl, RTSP_AV_DATA *pData)
{
	int nRet = 0;
	R_GSF_BC_HANDLE_S *pbcHdl = (R_GSF_BC_HANDLE_S *)Hdl;
	if(pbcHdl == NULL)
	{
		printf("backchannel send pbcHdl NULL,error\n");
		return BACKCHANNEL_CB_PARAM_ERR_E;
	}
	
	if(gBackchannelM.state == 1)
	{
		//printf("callback RECV hdl(%d) data[%p],size[%ld],Type[%d]\n", *pHdl, pData->data, pData->u32Size, pData->eType);
		//send data
		nRet = gsf_rtsp_backchannel_senddata(pbcHdl->nSocket, pData);
		if(nRet != pData->u32Size)
		{
			printf("send tcp data error nRet(%d),u32Size(%d)\n",nRet,pData->u32Size);
			return BACKCHANNEL_CB_SEND_ERR_E;
		}
		else
		{
			return BACKCHANNEL_CB_OK_E;
		}
		//printf("body[%s]\n",pData->data);
	}
	else
	{
		printf("backchannel send error BUSY(%d), socket(%d)error \n", gBackchannelM.state ,pbcHdl->nSocket);
		nRet = BACKCHANNEL_CB_STATE_ERR_E;
	}

	return nRet;
}

//close
static int gsf_rtsp_backchannel_close(RTSP_AV_HDL Hdl)
{
	int nRet = 0;
	R_GSF_BC_HANDLE_S *pbcHdl = (R_GSF_BC_HANDLE_S *)Hdl;
	if(pbcHdl == NULL)
	{
		printf("backchannel close pbcHdl NULL,error\n");
		return BACKCHANNEL_CB_PARAM_ERR_E;
	}

	if(gBackchannelM.state == 1  && pbcHdl->nSocket)
	{
		printf("close backchannel socket(%d) \n",  pbcHdl->nSocket);
		//close handle socket
		nRet = gsf_rtsp_backchannel_closeconnect(pbcHdl->nSocket);
		if(nRet != 0)
		{
			printf("close backchannel socket(%d) error nRet(%d)\n", pbcHdl->nSocket, nRet);
			nRet = BACKCHANNEL_CB_CLOSE_ERR_E;
		}
		gBackchannelM.state = 0;
		nRet = BACKCHANNEL_CB_OK_E;
	}
	else
	{
		printf("backchannel close error BUSY(%d), socket(%d)error\n", gBackchannelM.state, pbcHdl->nSocket);
		gBackchannelM.state = 0;
		nRet = BACKCHANNEL_CB_STATE_ERR_E;
	}
	
	if(pbcHdl)
		free(pbcHdl);
	return nRet;
}


/*****************
*��Ƶ���ݻص�������
*
*������:_gsf_rtsp_back_channel
*��������:rtsp back channel ������Ƶ���ݻص�������
*ͨ��tcp��������3322�˿ڽ���tcp����,����backchannel����
*����:pHdl,handle;
*nType,��������:
*open:�� backchannel������,pHdlָ��ֵ����0,����ɹ�����0
*recv, ����backchannel������,pHdl ,nType backchannel
*close,�ر�backchannel����handle ,
*
******************/
int _gsf_rtsp_back_channel(RTSP_AV_HDL *pHdl, RTSP_BC_TYPE nType, RTSP_AV_DATA *pData, void *u)
{

	int nRet = 0;
	RTSP_AV_HDL hdl = 0;
	
	if(pHdl == NULL)
	{
		nRet = BACKCHANNEL_CB_PARAM_ERR_E;
		goto _done_error;
	}
	if(nType == RTSP_BC_OPEN)		//open handle proc
	{
		nRet = gsf_rtsp_backchannel_open(pHdl);
	}
	else if(nType == RTSP_BC_RECV)			//recv handle proc
	{
		//check pdata
		if(pData == NULL)
		{
			nRet = BACKCHANNEL_CB_PARAM_ERR_E;
			goto _done_error;
		}
		if(pHdl == NULL || *pHdl <= 0)
		{
			nRet = BACKCHANNEL_CB_PARAM_ERR_E;
			goto _done_error;
		}
		hdl = *pHdl;
		nRet = gsf_rtsp_backchannel_send(hdl, pData);
	}
	else if(nType == RTSP_BC_CLOSE)			//close handle proc
	{
		if(pHdl == NULL || *pHdl <= 0)
		{
			nRet = BACKCHANNEL_CB_PARAM_ERR_E;
			goto _done_error;
		}
		hdl = *pHdl;
		nRet = gsf_rtsp_backchannel_close(hdl);
		pHdl = NULL;
	}
	else
	{
		//printf("RTSP_BC_TYPE error nType(%d)\n",nType);
		nRet = BACKCHANNEL_CB_PARAM_ERR_E;	
	}

	//�ص�����ɹ�
	_done_error:
		return nRet;
		
	return BACKCHANNEL_CB_OK_E;
}

#endif


int _gsf_rtsp_ref_user_data(void **u)
{
	printf("[%s][%d]--debug-- _gsf_rtsp_ref_user_data,u(%p), *u(%p)\n", __FUNCTION__, __LINE__, u, *u);

	if(*u == NULL)
	{
		RTSP_USER_DATA_T *pUserData = NULL;
		pUserData = (RTSP_USER_DATA_T *) malloc(sizeof(RTSP_USER_DATA_T));
		if(NULL == pUserData)
		{
			printf("[%s][%d]--debug-- _gsf_rtsp_ref_user_data malloc pUserData error,return -1\n", __FUNCTION__,__LINE__);
			return -1;
		}
		memset(pUserData, 0, sizeof(RTSP_USER_DATA_T));
		pUserData->nhandle = (int)pUserData;
		printf("[%s][%d]--debug-- malloc pUserData(%p),nHandle(%x)\n", __FUNCTION__, __LINE__, pUserData, pUserData->nhandle);
	
		*u = pUserData;
	}else
	{
		printf("[%s][%d]--debug-- _gsf_rtsp_ref_user_data u[%p]\n",__FUNCTION__, __LINE__, u);
	}
	return 0;
}
int _gsf_rtsp_unref_user_data(void *u)
{
	printf("[%s][%d]_gsf_rtsp_unref_user_data,u(%p)\n", __FUNCTION__, __LINE__, u);
	if(NULL == u)
	{
		printf("[%s][%d]_gsf_rtsp_unref_user_data,unref error\n", __FUNCTION__, __LINE__);
		return -1;
	}
	RTSP_USER_DATA_T *pUserData = (RTSP_USER_DATA_T *)u;
	free(pUserData);
	pUserData = NULL;
	
	printf("[%s][%d]_gsf_rtsp_unref_user_data,unref ok\n", __FUNCTION__, __LINE__);
	return 0;
}


static int _rtsp_api_get(GSF_RTSP_PLAY_CB *pCb)
{
  pCb->funcCheckUser      = _gsf_rtsp_check_usr_pwd;
  pCb->funcNotify         = _gsf_rtsp_notify;
  pCb->funcOpenStream     = _gsf_rtsp_open_stream;
  pCb->funcOpenAlarm      = _gsf_rtsp_open_alarm;
  pCb->funcCloseStream    = _gsf_rtsp_close_stream;
  pCb->funcCloseAlarm     = _gsf_rtsp_close_alarm;
  pCb->funcVencCtl        = _gsf_rtsp_venc_ctl;
  pCb->funcBufferCtl      = _gsf_rtsp_buffer_ctl;
  pCb->funcGetVideoStream = _gsf_rtsp_get_video_data;
  pCb->funcGetAudioStream = _gsf_rtsp_get_audio_data;
  pCb->funcGetAlarmStream = _gsf_rtsp_get_alarm_frame;

	pCb->funcWriteLog       = _gsf_rtsp_log;
	pCb->funcGetMulticast   = _gsf_rtsp_get_multicast_config;
	pCb->funcOpenRecord		= _gsf_rtsp_open_rec;
	pCb->funcCloseRecord	= _gsf_rtsp_close_rec;
	pCb->funcGetRecordData= _gsf_rtsp_get_rec_frame;
	pCb->funcRecordCtl    = _gsf_rtsp_rec_ctl;
	pCb->funcGetRecSegment  = _gsf_rtsp_get_rec_segment;
	pCb->funcBackChannel = _gsf_rtsp_back_channel;

	pCb->funcRefUserData = _gsf_rtsp_ref_user_data;
	pCb->funcUnRefUserData = _gsf_rtsp_unref_user_data;
	//pCb->funcGetRealDesc = _gsf_rtsp_get_real_desc;		//res ����
	
	return 0;
}

static int _rtsp_cfg_get(GSF_RTSP_CFG *pstCfg)
{
	int nRet = -1;
  pstCfg->nMaxCh = 1;
  pstCfg->nMaxCh        = 1;
  pstCfg->nStreamNum    = 3;
  pstCfg->nRtspPort     = 555;
  pstCfg->bUseAuth      = 0;        // �Ƿ�����У�� 0 ������ 1����basic 2 digest
  pstCfg->bRtspMode     = 0;
  pstCfg->bRtspKeepAlive= 0;
  pstCfg->u8RtspUdpTBF  = 0;        //TBF 0: disable, 1: auto, 2 manual
  pstCfg->u32TBFCps     = 0;        // 100 - 100000 k
	pstCfg->u32TBFBurst   = 0;        // 100 - 100000 k
	pstCfg->U32TBFBurstDuration  = 0; // 10  - 1000   ms
    
	return 0;
}

void _rtsp_restart()
{
	pthread_mutex_lock(&s_mutexRtsp);
	
	_rtsp_api_get(&stRtspPlayCb);
	_rtsp_cfg_get(&stRtspCfg);
  char _log[256] = {0};
	sprintf(_log,
	     ">>> nMaxCh:     %d \n"
		   ">>> nStreamNum: %d \n"
		   ">>> nRtspPort:  %d \n"
		   ">>> bUseAuth:   %d \n"
		   ">>> bRtspMode:  %d \n"
		   ">>> bRtspKeepAlive: %d \n"
		   ">>> u8RtspUdpTBF: %d , cps: %d k, burst: %d k, Duration: %d ms\n"
		, stRtspCfg.nMaxCh
		, stRtspCfg.nStreamNum
		, stRtspCfg.nRtspPort
		, stRtspCfg.bUseAuth  //
		, stRtspCfg.bRtspMode // δ�õ�
		, stRtspCfg.bRtspKeepAlive //
		, stRtspCfg.u8RtspUdpTBF, stRtspCfg.u32TBFCps, stRtspCfg.u32TBFBurst, stRtspCfg.U32TBFBurstDuration
		);
		
	_gsf_rtsp_log(_log);

	gsf_rtsp_stop();
	gsf_rtsp_start(&stRtspCfg, &stRtspPlayCb);

	pthread_mutex_unlock(&s_mutexRtsp);
	
}

static void _rtsp_main_exit()
{
	pthread_mutex_lock(&s_mutexRtsp);
	
	gsf_rtsp_stop();

	pthread_mutex_unlock(&s_mutexRtsp);

	exit(0);
}

static void _rtsp_main_signal()
{
	signal(SIGPIPE, SIG_IGN);

	signal(SIGTERM, _rtsp_main_exit);
	signal(SIGINT,  _rtsp_main_exit);
}


int rtsp_server_start()
{
	_rtsp_main_signal();
	_rtsp_restart();	
	return 0;
}
