#ifndef _IOF_ARCH_H_
#define _IOF_ARCH_H_
#include "gagent_typedef.h"
#include "platform.h"
//return wifistatus. 
uint16 GAgent_DevCheckWifiStatus( void );
void CoreInit( void );
void GAgent_DevReset();
void GAgent_DevInit( pgcontext pgc );
void GAgent_DevTick( void );
uint32 GAgent_GetDevTime_MS();
uint32 GAgent_GetDevTime_S();
int8 GAgent_DevGetMacAddress( uint8* szmac );
void GAgent_DevLED_Red( uint8 onoff );
void GAgent_DevLED_Green( uint8 onoff );

uint32 GAgent_DevGetConfigData( gconfig *pConfig );
uint32 GAgent_DevSaveConfigData( gconfig *pConfig);

void GAgent_LocalDataIOInit( pgcontext pgc );


/*********Net function************/
uint32 GAgent_GetHostByName( int8 *domain, int8 *IPAddress);
int32 GAgent_accept(int32 sockfd );
int32 GAgent_listen(int32 sockfd, int32 backlog);
uint32 GAgent_sendto(int32  sockfd,  const  void  *buf, int32 len,  int32 flags);


                
/****************************************************************
*   functionName    :   GAgent_connect
*   flag            :   1 block 
*                       0 no block 
*   return          :   0> successful socketid
                         other fail.
****************************************************************/
int32 GAgent_connect( int32 iSocketId, uint16 port,
                        int8 *ServerIpAddr,int8 flag);

int8 GAgent_DRVGetWiFiMode( pgcontext pgc );
int16 GAgent_DRV_WiFi_SoftAPModeStart( const int8* ap_name,const int8 *ap_password,int16 wifiStatus );
int16 GAgent_DRVWiFi_StationCustomModeStart(int8 *StaSsid,int8 *StaPass,uint16 wifiStatus );
void DRV_ConAuxPrint( char *buffer, int len, int level );


/****************************************************************
        FunctionName        :   GAgent_OpenUart.
        Description         :   Open uart.
        BaudRate            :   The baud rate.
        number              :   The number of data bits.
        parity              :   The parity(0: none, 1:odd, 2:evn).
        StopBits            :   The number of stop bits .
        FlowControl         :   support flow control is 1
        return              :   uart fd.
****************************************************************/
int32 GAgent_OpenUart( int32 BaudRate,int8 number,int8 parity,int8 stopBits,int8 flowControl );
#endif
