#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <pthread.h>

#include "mpp.h"

typedef struct {
  int adec;
  int ao;
  // ...
}vo_audio_t;

typedef struct {
  int en;
  int depth;
  VO_WBC_MODE_E   mode;
  VO_WBC_ATTR_S   attr;
  VO_WBC_SOURCE_S src;
  // ...
}vo_wbc_t;

typedef struct {
  int vdec;
  int vpss;
  // ...
}vo_ch_t;

typedef struct {
  RECT_S     rect;
  VO_SPLIT_E cnt;
  vo_ch_t    ch[VO_MAX_CHN_NUM];
  // ...
}vo_layer_t;

typedef struct {
  // lock;
  pthread_mutex_t lock;
  // vodev;
  VO_INTF_TYPE_E intf;
  VO_INTF_SYNC_E sync;
  
  vo_wbc_t wbc;
  vo_audio_t audio;
  
  // volayer;
  vo_layer_t pip;
  vo_layer_t video;
  
  // ...
}vo_mng_t;

static vo_mng_t vo_mng[VO_MAX_DEV_NUM];

//������Ƶ����豸
int gsf_mpp_vo_start(int vodev, VO_INTF_TYPE_E type, VO_INTF_SYNC_E sync)
{
  int err = 0;
  // start vo dev;
  // start ao dev;
  return err;
}

int gsf_mpp_vo_stop(int vodev)
{
  int err = 0;
  // stop vo ch;
  // stop ao dev;
  // stop vo dev;
  return err;
}

int gsf_mpp_vo_resolu(int vodev, VO_INTF_SYNC_E sync)
{
  int err = 0;
  // gsf_mpp_vo_stop
  // gsf_mpp_vo_split
  // ...
  // gsf_mpp_vo_start
  return err;
}

//WBCʹ��
int gsf_mpp_vo_wbc(int vodev, int en)
{
  int err = 0;
  return err;
}

//������ʾͨ��
int gsf_mpp_vo_split(int volayer, VO_SPLIT_E split, RECT_S *rect, int vdch[VO_SPLIT_BUTT])
{
  int err = 0;
  
  //stop vo ch;
  //stop vpss;
  //unbind vo to vpss;
  
  // start vo ch;
  // start vpss;
  // bind vo to vpss;
  if(vdch)
  {
    // ����֧�ֲ�ͬ��VPSS/VO��ͬһ��VDEC;
    // bind vpss to this vdch[];
  }
  else
  {
    // autogen vdch[]: 
    // VOLAYER_HD0: [0 - VPSS_MAX_GRP_NUM/2], VOLAYER_PIP: [VPSS_MAX_GRP_NUM/2 - VPSS_MAX_GRP_NUM];
    // bind vpss to vdch[];
    // ��vdchû�д���ʱ��Ҫ��ʱ�ӳٰ�???
  }
  
  return err;
}
//�ƶ�������ʾ����(λ��,��С)
int gsf_mpp_vo_move(int volayer, RECT_S *rect)
{
  int err = 0;
  // 
  return err;
}

//�ü�ͨ��Դͼ��������ʾͨ��(�ֲ��Ŵ�)
int gsf_mpp_vo_crop(int volayer, int ch, RECT_S *rect)
{
  int err = 0;
  // 
  return err;
}

//������Ƶ���ݵ�ָ��ch;
int gsf_mpp_vo_vsend(int volayer, int ch, char *data, gsf_mpp_frm_attr_t *attr)
{
  int err = 0;
  // start vdec;
  return err;
}

//��ȡ������ʾ״̬
int gsf_mpp_vo_stat(int volayer, int ch, gsf_mpp_vo_stat_t *stat)
{
  int err = 0;
  // 
  return err;
}
//���ý�����ʾFPS
int gsf_mpp_vo_setfps(int volayer, int ch, int fps)
{
  int err = 0;
  // 
  return err;
}
//���������ʾBUFF
int gsf_mpp_vo_clear(int volayer, int ch)
{
  int err = 0;
  // 
  return err;
}

//adec;
//���õ�ǰ��Ƶ����ͨ����(!=ch�����ݲ��ͽ���)
int gsf_mpp_vo_afilter(int vodev, int ch)
{
  int err = 0;
  return err;
}
//������Ƶ���ݽ���(ch��ʾ��Ƶ���ݵ�ͨ����)
int gsf_mpp_vo_asend(int vodev, int ch, char *data, gsf_mpp_frm_attr_t *attr)
{
  int err = 0;
  // start adec;
  return err;
}
//��������ͨ��
int gsf_mpp_venc_start(gsf_mpp_venc_t *venc)
{
  int err = 0;
  return err;
}
//ֹͣ����ͨ��
int gsf_mpp_venc_stop(gsf_mpp_venc_t *venc)
{
  int err = 0;
  return err;
}

int gsf_mpp_venc_ctl(int VencChn, int id, void *args)
{
  int err = 0;
  return err;
}

//���������߳�
int gsf_mpp_venc_recv(gsf_mpp_recv_t *recv)
{
  int err = 0;
  return err;
}
//ֹͣ�����߳�
int gsf_mpp_venc_dest()
{
  int err = 0;
  return err;
}

