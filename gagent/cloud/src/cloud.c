#include "gagent.h"
#include "http.h"
#include "mqttxpg.h"
#include "cloud.h"
/*
return 0 OTA SUCCESS
*/
uint32 GAgent_Cloud_OTAByUrl( int32 socketid,uint8 *downloadUrl )
{
    //TODO.
    return 1;
}
/****************************************************************
        FunctionName    :   GAgent_Cloud_SendData
        Description     :   send buf data to M2M server.
        return          :   0-ok 
                            other fail.
        Add by Alex.lin     --2015-03-17
****************************************************************/
uint32 GAgent_Cloud_SendData( pgcontext pgc,ppacket pbuf,int32 buflen )
{
    int8 ret = 0;

    if( isPacketTypeSet( pbuf->type,CLOUD_DATA_OUT ) == 1);
    {
        ret = MQTT_SenData( pgc->gc.DID,pbuf,buflen );
        GAgent_Printf(GAGENT_INFO,"Send date to cloud :len =%d ,ret =%d",buflen,ret );
        
        pbuf->type = SetPacketType( pbuf->type,CLOUD_DATA_OUT,0 );
    }
    return ret;
}
/****************************************************************
        Function        :   Cloud_InitSocket
        Description     :   init socket connect to server.
        iSocketId       :   socketid.
        p_szServerIPAddr:   server ip address like "192.168.1.1"
        port            :   server socket port.
        flag            :   =0 init socket in no block.
                            =1 init socket in block.
        return          :   >0 the cloud socket id 
                            <=0 fail.
****************************************************************/
int32 Cloud_InitSocket( int32 iSocketId,int8 *p_szServerIPAddr,int32 port,int8 flag )
{
    //static int32 lastC2CTime = -20;
    int32 ret=0;
    //struct sockaddr_t Msocket_address;

    ret = strlen( p_szServerIPAddr );
    
    if( ret<=0 || ret> 17 )
        return -1;

    GAgent_Printf(GAGENT_DEBUG,"socket connect cloud ip:%s .",p_szServerIPAddr);
    if( iSocketId > 0 )
    {
        GAgent_Printf(GAGENT_DEBUG, "Cloud socket need to close SocketID:[%d]", iSocketId );
        close( iSocketId );
        iSocketId = 0;
    }

    if( (iSocketId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<=0)
    {
        ERRORCODE
        GAgent_Printf(GAGENT_ERROR," Cloud socket init fail");
        return -1;
    }

    GAgent_Printf(GAGENT_DEBUG, "New cloud socketID [%d]",iSocketId);
    ret = GAgent_connect( iSocketId, port, p_szServerIPAddr,flag );

    if ( RET_SUCCESS!=ret )
    {
        ERRORCODE 
        close(iSocketId);
        iSocketId=-1;
        GAgent_Printf(GAGENT_ERROR, "Cloud socket connect fail with:%d", ret);
        return -3;
    }
    return iSocketId;
}
/****************************************************************
        Function    :   Cloud_ReqRegister
        description :   sent register data to cloud
        Input       :   NULL;
        return      :   0-send register data ok.
                        other fail.
        add by Alex.lin     --2015-03-02
****************************************************************/
uint32 Cloud_ReqRegister( pgcontext pgc )
{
    uint32 socket = 0;
    int8 ret = 0;

    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;

    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);
    pGlobalVar->rtinfo.waninfo.http_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.http_socketid,pConfigData->GServer_ip,80,0 );
    socket = pGlobalVar->rtinfo.waninfo.http_socketid ;
    
    if( socket<=0 )
        return RET_FAILED;
    
    ret = Http_POST( socket, HTTP_SERVER,pConfigData->wifipasscode,pGlobalVar->minfo.szmac,
                        pGlobalVar->mcu.product_key );
    
    if( RET_SUCCESS!=ret )
        return RET_FAILED;
     else 
        return RET_SUCCESS;
}
/* 
    will get the device id.
*/
int8 Cloud_ResRegister( uint8 *cloudConfiRxbuf,int32 buflen,int8 *pDID,int32 respondCode )
{
    int32 ret=0;
    
    if( 201 != respondCode)
        return RET_FAILED;
    ret = Http_Response_DID( cloudConfiRxbuf,pDID );
    if( RET_SUCCESS==ret )
    {
        return RET_SUCCESS;
    }
    else 
        return RET_FAILED;
}

uint32 Cloud_ReqGetFid( pgcontext pgc,enum OTATYPE_T type )
{
    int32 socket = 0;
    int8 ret = 0;
    uint8 *hver, *sver;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    
    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);
    pGlobalVar->rtinfo.waninfo.http_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.http_socketid,pConfigData->GServer_ip,80,0 );
    socket = pGlobalVar->rtinfo.waninfo.http_socketid;
    
    if( socket<=0 )
        return RET_FAILED;
    
    GAgent_Printf(GAGENT_DEBUG, "http socket connect OK with:%d", socket);
    switch( type )
    {
        case OTATYPE_WIFI:
                hver = WIFI_HARDVER;
                sver = WIFI_SOFTVAR;
            break;
        case OTATYPE_MCU:
                hver = pGlobalVar->mcu.hard_ver;
                sver = pGlobalVar->mcu.soft_ver;
            break;
        default:
            GAgent_Printf( GAGENT_WARNING,"GAgent OTA type is invalid! ");
            return RET_FAILED;
            break;
    }
    
    HTTP_DoGetTargetId( type,HTTP_SERVER,pConfigData->DID,pGlobalVar->mcu.product_key,
                        hver,sver,/*pConfigData->FirmwareId,*/socket );
    
}

/****************************************************************
*       FunctionName    :   Cloud_ResGetFid.
*       Description     :   get firmwarm download url and firmwarm version.
*       buf             :   data form cloud after req fid.
*       download_url    :   new firmwarm download url
*       fwver           :   new firmwarm version.
*       respondCode     :   http respond code.
*       reutn           :   0 success other error.
*       Add by Alex.lin   --2015-03-03
****************************************************************/
int8 Cloud_ResGetFid( uint8 *download_url, uint8 *fwver, uint8 *cloudConfiRxbuf,int32 respondCode )
{
    int32 ret=0;
    int32 target_fid=0;
    
    if( 200 != respondCode )
        return RET_FAILED;
    ret = Http_GetFid_Url( &target_fid,download_url,fwver, cloudConfiRxbuf );
    if( RET_SUCCESS != ret )
    {
        return RET_FAILED;
    }
    return RET_SUCCESS;
}

/****************************************************************
*       FunctionName    :   Cloud_ReqProvision
*       Description     :   send provision req to host.
*       host            :   GServer host,like "api.gizwits.com"
*       return          :   0 success other error.
*       Add by Alex.lin   --2015-03-03
****************************************************************/
uint32 Cloud_ReqProvision( pgcontext pgc )
{
    int32 socket = 0;
    int8 ret = 0;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    
    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);
    
    pGlobalVar->rtinfo.waninfo.http_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.http_socketid,pConfigData->GServer_ip,80,0 );
    socket = pGlobalVar->rtinfo.waninfo.http_socketid;
    if( socket<=0 )
        return RET_FAILED;
    
    ret = Http_GET( HTTP_SERVER,pConfigData->DID,socket );
    return ret;
}
/****************************************************************
*       FunctionName    :   Cloud_ResProvision.
*       Description     :   data form server after provision.
*       szm2mhost       :   m2m server like: "m2m.gizwits.com"
*       port            :   m2m port .
*       respondCode     :   http respond code.
*       return          :   0 success other fail.
****************************************************************/
uint32 Cloud_ResProvision( int8 *szdomain,int32 *port,uint8 *cloudConfiRxbuf,int32 respondCode )
{
    int32 ret = 0;
    if( 200 != respondCode )
        return RET_FAILED;
    ret = Http_getdomain_port( cloudConfiRxbuf,szdomain,port );
    return ret;
}
/****************************************************************
*       FunctionName    :   Cloud_isNeedOTA
*       sFV             :   soft version
*       return          :   1 do not need to OTA
*                           0 need to OTA.
*       Add by Alex.lin   --2015-03-03
****************************************************************/
uint32 Cloud_isNeedOTA( uint8 *sFV )
{
    int32 result=0;
    /* TODO */
    return 1;
   
    result = strcmp( WIFI_SOFTVAR,sFV );
    if( result>=0 )
        return 1;
    return 0;
    
}
/****************************************************************
        Function    :   Cloud_ReqConnect
        Description :   send req m2m connect packet.
        username    :   username.
        password    :   username.
        return      :   0: send req connect packet ok
                        other req connect fail.
        Add by Alex.lin     --2015-03-09
****************************************************************/
uint32 Cloud_ReqConnect( pgcontext pgc,const int8 *username,const int8 *password )
{
    int8 ret = 0;
    int32 socket = 0;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    int32 nameLen=0,passwordLen=0;

    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);

    nameLen = strlen( username );
    passwordLen = strlen( password );
    
    if( nameLen<=0 || nameLen>22 ) /* invalid name */
    {
        GAgent_Printf( GAGENT_WARNING," can't req to connect to m2m invalid name length !");
        return 1;
    }
    if( passwordLen<=0 || passwordLen>16 )/* invalid password */
    {
        GAgent_Printf( GAGENT_WARNING," can't req to connect to m2m invalid password length !");
        return 1;
    }
    GAgent_Printf( GAGENT_INFO,"Connect to server domain:%s port:%d",pGlobalVar->minfo.m2m_SERVER,pGlobalVar->minfo.m2m_Port );
    

    pGlobalVar->rtinfo.waninfo.m2m_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.m2m_socketid ,pConfigData->m2m_ip ,
                                                         pGlobalVar->minfo.m2m_Port,0 );
    socket = pGlobalVar->rtinfo.waninfo.m2m_socketid;

    if( socket<=0 )
    {
        GAgent_Printf(GAGENT_WARNING,"m2m socket :%d",socket);
        return RET_FAILED;
    }
    GAgent_Printf(GAGENT_DEBUG,"Cloud_InitSocket OK!");
    
    ret = Mqtt_Login2Server( socket,username,password );
    return ret;
}
/****************************************************************
        Function    :   Cloud_ResConnect
        Description :   handle packet form mqtt req connect 
        buf         :   data form mqtt.
        return      :   0: req connect ok
                        other req connect fail.
        Add by Alex.lin     --2015-03-09
****************************************************************/
uint32 Cloud_ResConnect( uint8* buf )
{
    int32 i=0;
    if( buf[3] == 0x00 )
    {
        if( (buf[0]!=0) && (buf[1] !=0) )
        {
        return 0;
        }
        else
        {
            GAgent_Printf( GAGENT_ERROR,"%s %s %d",__FILE__,__FUNCTION__,__LINE__ );
            GAgent_Printf( GAGENT_ERROR,"MQTT Connect res  fail ret =%d!!",buf[3] );
            return 1;
        }
    }
    GAgent_Printf( GAGENT_ERROR,"res connect fail with %d ",buf[3] );
        return buf[3];
}
uint32 Cloud_ReqSubTopic( pgcontext pgc,uint16 mqttstatus )
{
  int32 ret=0;
  ret = Mqtt_DoSubTopic( pgc,mqttstatus);
  return ret;
}
/****************************************************************
        Function        :   Cloud_ResSubTopic
        Description     :   check sub topic respond.
        buf             :   data form mqtt.
        msgsubId        :   sub topic messages id
        return          :   0 sub topic ok.
                            other fail.
        Add by Alex.lin     --2015-03-09
****************************************************************/
uint32 Cloud_ResSubTopic( const uint8* buf,int8 msgsubId )
{
    uint16 recmsgId=0;
    recmsgId = mqtt_parse_msg_id( buf );
    if( recmsgId!=msgsubId )
        return 1;
    else 
        return 0;
}

uint32 Cloud_Disconnect()
{

}

uint32 Cloud_ReqDisable( pgcontext pgc )
{
    int32 ret = 0;
    int32 socket = 0;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    
    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);

    pGlobalVar->rtinfo.waninfo.http_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.http_socketid,pConfigData->GServer_ip,80,0 );
    socket = pGlobalVar->rtinfo.waninfo.http_socketid;

    if( socket<=0 )
        return RET_FAILED;

    ret = Http_Delete( socket,HTTP_SERVER,pConfigData->old_did,pConfigData->old_wifipasscode );
    return 0;
}
uint32 Cloud_ResDisable( int32 respondCode )
{
    if( 200 != respondCode )
        return 1;
    return 0;
}


uint32 Cloud_JD_Post_ReqFeed_Key( pgcontext pgc )
{
    int32 ret = 0;
    int32 socket = 0;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    
    pGlobalVar=pgc;
    pConfigData = &(pgc->gc);
    
    pGlobalVar->rtinfo.waninfo.http_socketid = Cloud_InitSocket( pGlobalVar->rtinfo.waninfo.http_socketid,pConfigData->GServer_ip,80,0 );
    socket = pGlobalVar->rtinfo.waninfo.http_socketid;
    
    if( socket<=0 )
        return RET_FAILED;
    
    ret = Http_JD_Post_Feed_Key_req( socket,pConfigData->cloud3info.jdinfo.feed_id,pConfigData->cloud3info.jdinfo.access_key,
                                     pConfigData->DID,HTTP_SERVER );
    pConfigData->cloud3info.jdinfo.ischanged=0;
    GAgent_DevSaveConfigData( pConfigData );
    return 0;
}

uint32 Cloud_JD_Post_ResFeed_Key( pgcontext pgc,int32 respondCode )
{
    int ret=0;
    pgconfig pConfigData=NULL;
    
    pConfigData = &(pgc->gc);
    if( 200 != respondCode )
     return 1;
    
    if( 1 == pConfigData->cloud3info.jdinfo.ischanged )
    {
        GAgent_Printf(GAGENT_WARNING,"jd info is changed need to post again.");
        pConfigData->cloud3info.jdinfo.tobeuploaded=1;
        ret=1;
    }
    else
    {
        GAgent_Printf(GAGENT_DEBUG,"jd info post ok.");
        pConfigData->cloud3info.jdinfo.tobeuploaded=0;
        ret=0;
    }
    GAgent_DevSaveConfigData( pConfigData );
    return ret;
}
/****************************************************************
*       functionname    :   Cloud_ReadGServerConfigData
*       description     :   read data form gserver.
*       socket          :   gserver socket.
*       buf             :   data pointer form gserver
*       buflen          :   want to read data length
        return          :   >0 data form gserver
                            other error.
        Add by Alex.lin     --2015-03-03
****************************************************************/
int32 Cloud_ReadGServerConfigData( pgcontext pgc ,int32 socket,uint8 *buf,int32 buflen )
{
    int32 ret =0;
    ret = Http_ReadSocket( socket,buf,buflen );
    if( ret <0 ) 
    {
        GAgent_Printf( GAGENT_WARNING,"Cloud_ReadGServerConfigData fail close the socket:%d",socket );
        close( socket );
        socket = 0;
        GAgent_SetGServerSocket( pgc,socket );
        return -1;
    }
    return ret;
}


/****************************************************************
        FunctionName        :   GAgent_CloudTick.
        Description         :   GAgent Send cloud heartbeat to cloud
                                when mqttstatus is MQTT_STATUS_RUNNING

        Add by Alex.lin     --2015-03-10
****************************************************************/
void GAgent_CloudTick( pgcontext pgc )
{
    uint32 cTime=0,dTime=0;

    if( pgc->rtinfo.waninfo.mqttstatus != MQTT_STATUS_RUNNING )
        return;
    
    cTime = GAgent_GetDevTime_MS();
    dTime = abs(cTime - pgc->rtinfo.waninfo.send2MqttLastTime );
    
    if( dTime < CLOUD_HEARTBEAT )
        return ;
    
    if(pgc->rtinfo.waninfo.cloudPingTime > 2 )
    {
        ERRORCODE
        pgc->rtinfo.waninfo.cloudPingTime=0;
        GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_START );
        GAgent_SetWiFiStatus( pgc,WIFI_CLOUD_STATUS,1 );
    }
    else
    {
        MQTT_HeartbeatTime();
        pgc->rtinfo.waninfo.cloudPingTime++;
        GAgent_Printf(GAGENT_INFO,"GAgent MQTT Ping ...");
    }
    pgc->rtinfo.waninfo.send2MqttLastTime = GAgent_GetDevTime_MS();

}
/****************************************************************
*
*   function    :   gagent do cloud config.
*   cloudstatus :   gagent cloud status.
*   return      :   0 successful other fail.
*   Add by Alex.lin --2015-02-28
****************************************************************/
uint32 Cloud_ConfigDataHandle( pgcontext pgc /*int32 cloudstatus*/ )
{
  int32 dTime=0;
  int32 ret =0;
  int32 respondCode=0;
  int32 cloudstatus = 0;
  pgcontext pGlobalVar=NULL;
  pgconfig pConfigData=NULL;

  uint16 GAgentStatus = 0;
  uint8 cloudConfiRxbuf[1024]={0};
  int8 *pDeviceID=NULL;

  fd_set readfd;
  int32 http_fd;

  pConfigData = &(pgc->gc);
  pGlobalVar = pgc;
  cloudstatus = pgc->rtinfo.waninfo.CloudStatus;
  GAgentStatus = pgc->rtinfo.GAgentStatus;

  if( (GAgentStatus&WIFI_STATION_STATUS)!= WIFI_STATION_STATUS)
    return 1 ;
  if( strlen( pgc->gc.GServer_ip)>IP_LEN || strlen( pgc->gc.GServer_ip)<7 )
  {
    GAgent_Printf( GAGENT_WARNING,"GServer IP is NULL!!");
    return 1;
  }
  if( cloudstatus== CLOUD_CONFIG_OK )
  {
      if( pGlobalVar->rtinfo.waninfo.http_socketid >0 )
      {
        GAgent_Printf( GAGENT_CRITICAL,"http config ok ,and close the socket.");
        close( pGlobalVar->rtinfo.waninfo.http_socketid );
        pGlobalVar->rtinfo.waninfo.http_socketid=-1;
      }
      return 1;
  }
  pDeviceID = pConfigData->DID;
  http_fd = pGlobalVar->rtinfo.waninfo.http_socketid;
  readfd  = pGlobalVar->rtinfo.readfd;
  
  if( CLOUD_INIT == cloudstatus )
  {
        GAgent_Printf( GAGENT_INFO,"%s %d",__FUNCTION__,__LINE__ );
        if( strlen(pDeviceID)==22 )/*had did*/
        {
            GAgent_Printf( GAGENT_INFO,"Had did !!!!" );
            ret = Cloud_ReqGetFid( pgc,OTATYPE_WIFI );
            GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_GET_TARGET_FID);
        }
        else
        {
            GAgent_Printf( GAGENT_INFO,"Need to get did!!!" );
            GAgent_SetDeviceID( pgc,NULL );//clean did.
            ret = Cloud_ReqRegister( pgc );
            GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_GET_DID );
        }
        return 0;
  }

  dTime = abs( GAgent_GetDevTime_MS()- pGlobalVar->rtinfo.waninfo.send2HttpLastTime );

  if( FD_ISSET( http_fd,&readfd )||( (cloudstatus<CLOUD_CONFIG_OK) && (dTime>GAGENT_HTTP_TIMEOUT)) )
  {
   GAgent_Printf(GAGENT_DEBUG,"HTTP Data from Gserver!%d", 2);
   ret = Cloud_ReadGServerConfigData( pgc,pGlobalVar->rtinfo.waninfo.http_socketid,cloudConfiRxbuf,1024 );
   respondCode = Http_Response_Code( cloudConfiRxbuf );
      
   GAgent_Printf(GAGENT_INFO,"http read ret:%d cloudStatus : %d",ret,cloudstatus );
   GAgent_Printf(GAGENT_INFO,"http response code : %d",respondCode );
   switch( cloudstatus )
   {
        case CLOUD_RES_GET_DID:
             ret = Cloud_ResRegister( cloudConfiRxbuf,ret,pDeviceID,respondCode );
             if( (RET_SUCCESS != ret) )/* can't got the did */
             {
                if(dTime>GAGENT_HTTP_TIMEOUT)
                {
                   GAgent_Printf(GAGENT_ERROR,"res register fail: %s %d",__FUNCTION__,__LINE__ );
                   GAgent_Printf(GAGENT_ERROR,"go to req register Device id again.");
                   ret = Cloud_ReqRegister( pgc );
                }
             }
             else
             {
                GAgent_SetDeviceID( pgc,pDeviceID );
                GAgent_DevGetConfigData( &(pgc->gc) );
                GAgent_Printf( GAGENT_DEBUG,"Register got did :%s len=%d",pgc->gc.DID,strlen(pgc->gc.DID) );
                 
                ret = Cloud_ReqGetFid( pgc,OTATYPE_WIFI );
                GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_GET_TARGET_FID ); 
             }
             break;
        case CLOUD_RES_GET_TARGET_FID:
        {
            /*
              获取OTA信息错误进入provision 成功则进行OTA.
            */
             int8 download_url[256]={0};
             ret = Cloud_ResGetFid( download_url ,pGlobalVar->rtinfo.waninfo.firmwarever ,cloudConfiRxbuf,respondCode );
             if( RET_SUCCESS != ret )
             {
                 GAgent_Printf( GAGENT_WARNING,"GAgent get OTA info fail! go to provison! ");
                 ret = Cloud_ReqProvision( pgc );
                 GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_PROVISION );
                 break;
             }
             else
             {
				ret = Cloud_isNeedOTA( NULL );
				if( 0==ret )
				{
					GAgent_Cloud_OTAByUrl( http_fd,download_url );
				}
                GAgent_Printf(GAGENT_INFO," CLOUD_RES_GET_TARGET_FID OK!!");
                GAgent_Printf(GAGENT_INFO,"url:%s",download_url);
                /* go to provision */
                ret = Cloud_ReqProvision( pgc );
                GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_PROVISION );
             }
             
             break;
        }
        case CLOUD_RES_PROVISION:
             pGlobalVar->rtinfo.waninfo.Cloud3Flag = Http_Get3rdCloudInfo( pConfigData->cloud3info.cloud3Name,pConfigData->cloud3info.jdinfo.product_uuid ,
                                                            cloudConfiRxbuf );
             /* have 3rd cloud info need save to falsh */
             if( pGlobalVar->rtinfo.waninfo.Cloud3Flag == 1 )
             {
                GAgent_Printf(GAGENT_INFO,"3rd cloud name:%s",pConfigData->cloud3info.cloud3Name );
                GAgent_Printf(GAGENT_INFO,"3re cloud UUID: %s",pConfigData->cloud3info.jdinfo.product_uuid);
                GAgent_DevSaveConfigData( pConfigData );
             }
             ret = Cloud_ResProvision( pGlobalVar->minfo.m2m_SERVER , &pGlobalVar->minfo.m2m_Port,cloudConfiRxbuf,respondCode);
             if( ret!=0 )
             {
                
                if(dTime>GAGENT_HTTP_TIMEOUT)
                {
                   GAgent_Printf(GAGENT_WARNING,"Provision res fail ret=%d.", ret );
                   GAgent_Printf(GAGENT_WARNING,"go to provision again.");
                   ret = Cloud_ReqProvision( pgc );
                }
             }
             else
             {
                GAgent_Printf(GAGENT_INFO,"Provision OK!");
                GAgent_Printf(GAGENT_INFO,"M2M host:%s port:%d",pGlobalVar->minfo.m2m_SERVER,pGlobalVar->minfo.m2m_Port);
                GAgent_Printf(GAGENT_INFO,"GAgent go to login M2M !");
                GAgent_SetCloudConfigStatus( pgc,CLOUD_CONFIG_OK );
                //login to m2m.
                GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_START );
                if( 1==GAgent_IsNeedDisableDID( pgc ) )
                {
                    GAgent_Printf(GAGENT_INFO,"Need to Disable Device ID!");
                    ret = Cloud_ReqDisable( pgc );
                    GAgent_SetCloudConfigStatus( pgc,CLOUD_RES_DISABLE_DID );
                    break;
                }
              }
             break;
        case CLOUD_RES_DISABLE_DID:
             ret = Cloud_ResDisable( respondCode );
             if(ret!=0)
             {
                 if(dTime>GAGENT_HTTP_TIMEOUT)
                 {
                    GAgent_Printf(GAGENT_WARNING,"Disable Device ID Fail.");
                 }
                 else
                 {
                    GAgent_SetCloudConfigStatus ( pgc,CLOUD_CONFIG_OK );
                 }
             }
             else
             {
                GAgent_Printf(GAGENT_INFO,"Disable Device ID OK!");
                GAgent_SetOldDeviceID( pgc,NULL,NULL,0 );
                GAgent_SetCloudConfigStatus ( pgc,CLOUD_CONFIG_OK );
             }
              
             break;
        case CLOUD_RES_POST_JD_INFO:
             ret = Cloud_JD_Post_ResFeed_Key( pgc,respondCode );
             if( ret!=0 )
             {
                 GAgent_Printf( GAGENT_WARNING," Post JD info respond fail!" );
                 if( dTime>GAGENT_HTTP_TIMEOUT )
                 {
                    GAgent_Printf( GAGENT_WARNING," Post JD info again");
                    ret = Cloud_JD_Post_ReqFeed_Key( pgc );
                 }
             }
             else
             {
                GAgent_SetCloudConfigStatus( pgc,CLOUD_CONFIG_OK );
             }
             break;
    }
        pGlobalVar->rtinfo.waninfo.send2HttpLastTime = GAgent_GetDevTime_MS();   
  }
}

/****************************************************************
        FunctionName        :   Cloud_M2MDataHandle.
        Description         :   Receive cloud business data .
        xpg                 :   global context.
        Rxbuf                :   global buf struct.
        buflen              :   receive max len.
        return              :   >0 have business data,and need to 
                                   handle.
                                other,no business data.
        Add by Alex.lin     --2015-03-10
****************************************************************/
int32 Cloud_M2MDataHandle(  pgcontext pgc,ppacket pbuf /*, ppacket poutBuf*/, uint32 buflen)
{
    uint32 dTime=0,ret=0,dataLen=0;
    uint32 packetLen=0;
    pgcontext pGlobalVar=NULL;
    pgconfig pConfigData=NULL;
    int8 *username=NULL;
    int8 *password=NULL;
    uint8* pMqttBuf=NULL;
    fd_set readfd;
    int32 mqtt_fd=0;
    uint16 mqttstatus=0;
    uint8 mqttpackType=0;
    
    pConfigData = &(pgc->gc);
    pGlobalVar = pgc;
    pMqttBuf = pbuf->phead;
    
    mqttstatus = pGlobalVar->rtinfo.waninfo.mqttstatus;
    mqtt_fd = pGlobalVar->rtinfo.waninfo.m2m_socketid;
    readfd  = pGlobalVar->rtinfo.readfd;
    username = pConfigData->DID;
    password = pConfigData->wifipasscode;
    
    if( strlen(pConfigData->m2m_ip)==0 )
    {
        //GAgent_Printf( GAGENT_INFO,"M2M IP =0 IP TIME 1 %d 2%d ",pgc->rtinfo.waninfo.RefreshIPLastTime,pgc->rtinfo.waninfo.RefreshIPTime);
        return 0;
    }
    if( MQTT_STATUS_START==mqttstatus )
    {
        GAgent_Printf(GAGENT_INFO,"Req to connect m2m !");
        GAgent_Printf(GAGENT_INFO,"username: %s password: %s",username,password);

        Cloud_ReqConnect( pgc,username,password );
        GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_RES_LOGIN );
        GAgent_Printf(GAGENT_INFO," MQTT_STATUS_START ");
        pgc->rtinfo.waninfo.send2MqttLastTime = GAgent_GetDevTime_MS();
        return 0;
    }
    dTime = abs( GAgent_GetDevTime_MS()-pGlobalVar->rtinfo.waninfo.send2MqttLastTime );
    if( FD_ISSET( mqtt_fd,&readfd )||( mqttstatus!=MQTT_STATUS_RUNNING && dTime>GAGENT_MQTT_TIMEOUT))
    {
        if( FD_ISSET( mqtt_fd,&readfd ) )
        {
          GAgent_Printf(GAGENT_DEBUG,"Data form M2M!!!");
          resetPacket( pbuf );
          packetLen = MQTT_readPacket(mqtt_fd,pbuf,GAGENT_BUF_LEN );
          if( packetLen==-1 ) 
          {
              mqtt_fd=-1;
              pGlobalVar->rtinfo.waninfo.m2m_socketid=-1;
              GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_START );
              GAgent_Printf(GAGENT_DEBUG,"MQTT fd was closed!!");
              GAgent_Printf(GAGENT_DEBUG,"GAgent go to MQTT_STATUS_START");
              return -1;
          }
          else if(packetLen>0)
          {
            mqttpackType = MQTTParseMessageType( pMqttBuf );
            GAgent_Printf( GAGENT_DEBUG,"MQTT message type %d",mqttpackType );
          }
          else
          {
            return -1;
          }
        }

        /*create mqtt connect to m2m.*/
        if( MQTT_STATUS_RUNNING!=mqttstatus &&
            (MQTT_MSG_CONNACK==mqttpackType||MQTT_MSG_SUBACK==mqttpackType||dTime>GAGENT_MQTT_TIMEOUT))
        {
            switch( mqttstatus)
            {
                case MQTT_STATUS_RES_LOGIN:
                    
                     ret = Cloud_ResConnect( pMqttBuf );
                     if( ret!=0 )
                     {
                         GAgent_Printf(GAGENT_DEBUG," MQTT_STATUS_REQ_LOGIN Fail ");
                         if( dTime > GAGENT_MQTT_TIMEOUT )
                         {
                            GAgent_Printf(GAGENT_DEBUG,"MQTT req connetc m2m again!");
                            Cloud_ReqConnect( pgc,username,password );
                         }
                     }
                     else
                     {
                         GAgent_Printf(GAGENT_DEBUG,"GAgent do req connect m2m OK !");
                         GAgent_Printf(GAGENT_DEBUG,"Go to MQTT_STATUS_REQ_LOGINTOPIC1. ");
                         Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC1 );
                         GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_RES_LOGINTOPIC1 );
                     }
                     break;
                case MQTT_STATUS_RES_LOGINTOPIC1:
                     ret = Cloud_ResSubTopic(pMqttBuf,pgc->rtinfo.waninfo.mqttMsgsubid );
                     if( 0!=ret )
                     {
                        GAgent_Printf(GAGENT_DEBUG," MQTT_STATUS_RES_LOGINTOPIC1 Fail ");
                        if( dTime > GAGENT_MQTT_TIMEOUT )
                        {
                            GAgent_Printf( GAGENT_DEBUG,"GAgent req sub LOGINTOPIC1 again ");
                            Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC1 );
                        }
                     }
                     else
                     {
                        GAgent_Printf(GAGENT_DEBUG,"Go to MQTT_STATUS_RES_LOGINTOPIC2. ");
                        Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC2 );
                        GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_RES_LOGINTOPIC2 );
                     }
                     break;
                case MQTT_STATUS_RES_LOGINTOPIC2:
                     ret = Cloud_ResSubTopic(pMqttBuf,pgc->rtinfo.waninfo.mqttMsgsubid );
                     if( 0 != ret )
                     {
                        GAgent_Printf(GAGENT_DEBUG," MQTT_STATUS_RES_LOGINTOPIC2 Fail ");
                        if( dTime > GAGENT_MQTT_TIMEOUT )
                        {
                            GAgent_Printf( GAGENT_INFO,"GAgent req sub LOGINTOPIC2 again.");
                            Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC2 );
                        }
                     }
                     else
                     {
                        GAgent_Printf(GAGENT_DEBUG," Go to MQTT_STATUS_RES_LOGINTOPIC3. ");
                        Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC3 );
                        GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_RES_LOGINTOPIC3 );
                     }
                     break;
                case MQTT_STATUS_RES_LOGINTOPIC3:
                      ret = Cloud_ResSubTopic(pMqttBuf,pgc->rtinfo.waninfo.mqttMsgsubid );
                     if( ret != 0 )
                     {
                        GAgent_Printf(GAGENT_DEBUG," MQTT_STATUS_RES_LOGINTOPIC3 Fail ");
                        if( dTime > GAGENT_MQTT_TIMEOUT )
                        {
                            GAgent_Printf(GAGENT_DEBUG,"GAgent req sub LOGINTOPIC3 again." );
                            Cloud_ReqSubTopic( pgc,MQTT_STATUS_REQ_LOGINTOPIC3 );
                        }
                     }
                     else
                     {
                        GAgent_Printf(GAGENT_CRITICAL,"GAgent Cloud Working...");
                        GAgent_SetCloudServerStatus( pgc,MQTT_STATUS_RUNNING );
                        GAgent_SetWiFiStatus( pgc,WIFI_CLOUD_STATUS,0 );
                     }
                      break;
                default:
                     break;
            }
            pgc->rtinfo.waninfo.send2MqttLastTime = GAgent_GetDevTime_MS();  
        }
        else if( packetLen>0 && ( mqttstatus == MQTT_STATUS_RUNNING ) )
        {
            int varlen=0,p0datalen=0;
            switch( mqttpackType )
            {
                case MQTT_MSG_PINGRESP:
                    pgc->rtinfo.waninfo.cloudPingTime=0;
                    GAgent_Printf(GAGENT_INFO,"GAgent MQTT Pong ... \r\n");
                break;
                case MQTT_MSG_PUBLISH:
                    dataLen = Mqtt_DispatchPublishPacket( pgc,pMqttBuf,packetLen );
                    if( dataLen>0 )
                    {
                        pbuf->type = SetPacketType( pbuf->type,CLOUD_DATA_IN,1 );
                        ParsePacket(  pbuf );
                        GAgent_Printf(GAGENT_INFO,"%s %d type : %04X len :%d",__FUNCTION__,__LINE__,pbuf->type,dataLen );
                    }
                break;
                default:
                    GAgent_Printf(GAGENT_WARNING," data form m2m but msg type is %d",mqttpackType );
                break;
            }
        }
    }
    return dataLen;
}

int32 GAgent_Cloud_GetPacket( pgcontext pgc,ppacket pRxbuf, int32 buflen)
{
	int32 Mret=0,Hret=0;
	uint16 GAgentstatus = 0;
    ppacket pbuf = pRxbuf;
	GAgentstatus = pgc->rtinfo.GAgentStatus;
   
	if( (GAgentstatus&WIFI_STATION_STATUS)!= WIFI_STATION_STATUS)
	return -1 ;

	Hret = Cloud_ConfigDataHandle( pgc );
	Mret = Cloud_M2MDataHandle( pgc,pbuf, buflen );
	return Mret;
}
void GAgent_Cloud_Handle( pgcontext pgc, ppacket Rxbuf,int32 length )
{
    int32 cloudDataLen = 0;
    int32 ret=0;

    cloudDataLen = GAgent_Cloud_GetPacket( pgc,Rxbuf ,length );
    if( cloudDataLen>0 )
    {
        dealPacket(pgc, Rxbuf);        
    }
}