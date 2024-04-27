#include "string.h"
#include "eth_driver.h"
#include "MQTTPacket.h"

u8 MACAddr[6];                    // MAC address
u8 IPAddr[4] = {0, 0, 0, 0};      // IP address
u8 GWIPAddr[4] = {0, 0, 0, 0};    // Gateway IP address
u8 IPMask[4] = {0, 0, 0, 0};      // subnet mask
u8 DESIP[4] = {202, 61, 227, 61}; // MQTT server IP address,!!need to be modified manually

u8 SocketId;                    // socket id
u8 SocketRecvBuf[RECE_BUF_LEN]; // socket receive buffer
u8 MyBuf[RECE_BUF_LEN];
__attribute__((__aligned__(4))) u8 RemoteIp[4];
u16 desport = 1883; // MQTT server port
u16 srcport = 4200;
u16 DnsPort = 53; // source port

char *username = "vale";                                                             // Device name, unique for each device, available "/" for classification
char *password = "12c32aa281f4368da1ce692b6b2f71ab0942a8a3a07cfdd3ae8d12172b2c5a8d"; // Server login password
char *sub_topic = "topic/1";                                                         // subscribed session name
char *pub_topic = "topic/1";                                                         // Published session name
int pub_qos = 0;                                                                     // Publish quality of service
int sub_qos = 0;                                                                     // Subscription quality of service
char *payload = "WCHNET MQTT";                                                       // Publish content

u8 con_flag = 0;  // Connect MQTT server flag
u8 pub_flag = 0;  // Publish session message flag/
u8 sub_flag = 0;  // Subscription session flag
u8 tout_flag = 0; // time-out flag
u16 packetid = 5; // package id

/*********************************************************************
 * @fn      mStopIfError
 *
 * @brief   check if error.
 *
 * @param   iError - error constants.
 *
 * @return  none
 */
void mStopIfError(u8 iError)
{
    if (iError == WCHNET_ERR_SUCCESS)
        return;
    printf("Error: %02X\r\n", (u16)iError);
}

/*********************************************************************
 * @fn      WCHNET_CreateTcpSocket
 *
 * @brief   Create TCP Socket
 *
 * @return  none
 */
void WCHNET_CreateTcpSocket(void)
{
    u8 i;
    SOCK_INF TmpSocketInf;

    memset((void *)&TmpSocketInf, 0, sizeof(SOCK_INF));
    memcpy((void *)TmpSocketInf.IPAddr, DESIP, 4);
    TmpSocketInf.DesPort = desport;
    TmpSocketInf.SourPort = srcport++;
    TmpSocketInf.ProtoType = PROTO_TYPE_TCP;
    TmpSocketInf.RecvBufLen = RECE_BUF_LEN;
    i = WCHNET_SocketCreat(&SocketId, &TmpSocketInf);
    printf("WCHNET_SocketCreat %d\r\n", SocketId);
    mStopIfError(i);
    i = WCHNET_SocketConnect(SocketId); // make a TCP connection
    mStopIfError(i);
}

/*********************************************************************
 * @fn      WCHNET_DataLoopback
 *
 * @brief   Data loopback function.
 *
 * @param   id - socket id.
 *
 * @return  none
 */
void WCHNET_DataLoopback(u8 id)
{
#if 1
    u8 i;
    u32 len;
    u32 endAddr = SocketInf[id].RecvStartPoint + SocketInf[id].RecvBufLen; // Receive buffer end address

    if ((SocketInf[id].RecvReadPoint + SocketInf[id].RecvRemLen) > endAddr)
    { // Calculate the length of the received data
        len = endAddr - SocketInf[id].RecvReadPoint;
    }
    else
    {
        len = SocketInf[id].RecvRemLen;
    }
    i = WCHNET_SocketSend(id, (u8 *)SocketInf[id].RecvReadPoint, &len); // send data
    if (i == WCHNET_ERR_SUCCESS)
    {
        WCHNET_SocketRecv(id, NULL, &len); // Clear sent data
    }
#else
    u32 len, totallen;
    u8 *p = MyBuf, TransCnt = 255;

    len = WCHNET_SocketRecvLen(id, NULL); // query length
    printf("Receive Len = %d\r\n", len);
    totallen = len;
    WCHNET_SocketRecv(id, MyBuf, &len); // Read the data of the receive buffer into MyBuf
    while (1)
    {
        len = totallen;
        WCHNET_SocketSend(id, p, &len); // Send the data
        totallen -= len;                // Subtract the sent length from the total length
        p += len;                       // offset buffer pointer
        if (!--TransCnt)
            break; // Timeout exit
        if (totallen)
            continue; // If the data is not sent, continue to send
        break;        // After sending, exit
    }
#endif
}

/*********************************************************************
 * @fn      TIM2_Init
 *
 * @brief   Initializes TIM2.
 *
 * @return  none
 */
void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = SystemCoreClock / 1000000;
    TIM_TimeBaseStructure.TIM_Prescaler = WCHNETTIMERPERIOD * 1000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    NVIC_EnableIRQ(TIM2_IRQn);
}

/*********************************************************************
 * @fn      Transport_Open
 *
 * @brief   open the TCP connection.
 *
 * @return  socket id
 */
u8 Transport_Open(void)
{
    u8 i;
    SOCK_INF TmpSocketInf;

    memset((void *)&TmpSocketInf, 0, sizeof(SOCK_INF));
    memcpy((void *)TmpSocketInf.IPAddr, DESIP, 4);
    TmpSocketInf.DesPort = desport;
    TmpSocketInf.SourPort = srcport;
    TmpSocketInf.ProtoType = PROTO_TYPE_TCP;
    TmpSocketInf.RecvBufLen = RECE_BUF_LEN;
    i = WCHNET_SocketCreat(&SocketId, &TmpSocketInf);
    mStopIfError(i);

    i = WCHNET_SocketConnect(SocketId);
    mStopIfError(i);
    return SocketId;
}

/*********************************************************************
 * @fn      Transport_Close
 *
 * @brief   close the TCP connection.
 *
 * @return  @ERR_T
 */
u8 Transport_Close(void)
{
    u8 i;
    i = WCHNET_SocketClose(SocketId, TCP_CLOSE_NORMAL);
    mStopIfError(i);
    return i;
}

/*********************************************************************
 * @fn      Transport_SendPacket
 *
 * @brief   send data.
 *
 * @param   buf - data buff.
 *          len - data length
 *
 * @return  none
 */
void Transport_SendPacket(u8 *buf, u32 len)
{
    WCHNET_SocketSend(SocketId, buf, &len);
    printf("%d bytes uploaded!\r\n", len);
}

/*********************************************************************
 * @fn      MQTT_Connect
 *
 * @brief   Establish MQTT connection.
 *
 * @param   username - user name.
 *          password - password
 *
 * @return  none
 */
void MQTT_Connect(char *username, char *password)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    u32 len;
    u8 buf[200];

    data.clientID.cstring = "admin1";
    data.keepAliveInterval = 2000;
    data.cleansession = 1;
    data.username.cstring = username;
    data.password.cstring = password;

    len = MQTTSerialize_connect(buf, sizeof(buf), &data);
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      MQTT_Subscribe
 *
 * @brief   MQTT subscribes to a topic.
 *
 * @param   topic - Topic name to subscribe to.
 *          req_qos - quality of service
 *
 * @return  none
 */
void MQTT_Subscribe(char *topic, int req_qos)
{
    MQTTString topicString = MQTTString_initializer;
    u32 len;
    u32 msgid = 1;
    u8 buf[200];

    topicString.cstring = topic;
    len = MQTTSerialize_subscribe(buf, sizeof(buf), 0, msgid, 1, &topicString, &req_qos);
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      MQTT_Unsubscribe
 *
 * @brief   MQTT unsubscribe from a topic.
 *
 * @param   topic - Topic name to unsubscribe to.
 *
 * @return  none
 */
void MQTT_Unsubscribe(char *topic)
{
    MQTTString topicString = MQTTString_initializer;
    u32 len;
    u32 msgid = 1;
    u8 buf[200];

    topicString.cstring = topic;
    len = MQTTSerialize_unsubscribe(buf, sizeof(buf), 0, msgid, 1, &topicString);
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      MQTT_Publish
 *
 * @brief   MQTT publishes a topic.
 *
 * @param   topic - Published topic name.
 *          qos - quality of service
 *          payload - data buff
 *
 * @return  none
 */
void MQTT_Publish(char *topic, int qos, char *payload)
{
    MQTTString topicString = MQTTString_initializer;
    u32 payloadlen;
    u32 len;
    u8 buf[1024];

    topicString.cstring = topic;
    payloadlen = strlen(payload);
    len = MQTTSerialize_publish(buf, sizeof(buf), 0, qos, 0, packetid++, topicString, payload, payloadlen);
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      MQTT_Pingreq
 *
 * @brief   MQTT sends heartbeat packet
 *
 * @return  none
 */
void MQTT_Pingreq(void)
{
    u32 len;
    u8 buf[200];

    len = MQTTSerialize_pingreq(buf, sizeof(buf));
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      MQTT_Disconnect
 *
 * @brief   Disconnect the MQTT connection
 *
 * @return  none
 */
void MQTT_Disconnect(void)
{
    u32 len;
    u8 buf[50];
    len = MQTTSerialize_disconnect(buf, sizeof(buf));
    Transport_SendPacket(buf, len);
}

/*********************************************************************
 * @fn      msgDeal
 *
 * @brief   Dealing with subscription information.
 *
 * @param   msg - data buff
 *          len - data length
 *
 * @return  none
 */
void msgDeal(unsigned char *msg, int len)
{
    unsigned char *ptr = msg;
    printf("payload len = %d\r\n", len);
    printf("payload: ");
    for (int i = 0; i < len; i++)
    {
        printf("%c ", (u16)*ptr);
        ptr++;
    }
    printf("\r\n");
}

/*********************************************************************
 * @fn      WCHNET_HandleSockInt
 *
 * @brief   Socket Interrupt Handle
 *
 * @param   socketid - socket id.
 *          intstat - interrupt status
 *
 * @return  none
 */
void WCHNET_HandleSockInt(u8 socketid, u8 intstat)
{
    u32 len;
    int qos, payloadlen;
    MQTTString topicName;
    unsigned short packetid;
    unsigned char retained, dup;
    unsigned char *payload;

    if (intstat & SINT_STAT_RECV) // receive data
    {

        len = WCHNET_SocketRecvLen(socketid, NULL);
        WCHNET_SocketRecv(socketid, MyBuf, &len);
        switch (MyBuf[0] >> 4)
        {
        case CONNACK:
            printf("CONNACK\r\n");
            con_flag = 1;
            MQTT_Subscribe(sub_topic, sub_qos);
            break;

        case PUBLISH:
            MQTTDeserialize_publish(&dup, &qos, &retained, &packetid, &topicName,
                                    &payload, &payloadlen, MyBuf, len);
            msgDeal(payload, payloadlen);
            break;

        case SUBACK:
            sub_flag = 1;
            printf("SUBACK\r\n");
            break;

        default:

            break;
        }
        memset(MyBuf, 0, sizeof(MyBuf));
    }
    if (intstat & SINT_STAT_CONNECT) // connect successfully
    {
        WCHNET_ModifyRecvBuf(socketid, (u32)SocketRecvBuf, RECE_BUF_LEN);
        MQTT_Connect(username, password);
        printf("TCP Connect Success\r\n");
    }
    if (intstat & SINT_STAT_DISCONNECT) // disconnect
    {
        con_flag = 0;
        printf("TCP Disconnect\r\n");
    }
    if (intstat & SINT_STAT_TIM_OUT) // timeout disconnect
    {
        con_flag = 0;
        printf("TCP Timeout\r\n");
        Transport_Open();
    }
}

/*********************************************************************
 * @fn      WCHNET_HandleGlobalInt
 *
 * @brief   Global Interrupt Handle
 *
 * @return  none
 */
void WCHNET_HandleGlobalInt(void)
{
    u8 intstat;
    u16 i;
    u8 socketint;

    intstat = WCHNET_GetGlobalInt(); // get global interrupt flag
    if (intstat & GINT_STAT_UNREACH) // Unreachable interrupt
    {
        printf("GINT_STAT_UNREACH\r\n");
    }
    if (intstat & GINT_STAT_IP_CONFLI) // IP conflict
    {
        printf("GINT_STAT_IP_CONFLI\r\n");
    }
    if (intstat & GINT_STAT_PHY_CHANGE) // PHY status change
    {
        i = WCHNET_GetPHYStatus();
        if (i & PHY_Linked_Status)
            printf("PHY Link Success\r\n");
    }
    if (intstat & GINT_STAT_SOCKET) // socket related interrupt
    {
        for (i = 0; i < WCHNET_MAX_SOCKET_NUM; i++)
        {
            socketint = WCHNET_GetSocketInt(i);
            if (socketint)
                WCHNET_HandleSockInt(i, socketint);
        }
    }
}

/*********************************************************************
 * @fn      WCHNET_DNSCallBack
 *
 * @brief   DNSCallBack
 *
 * @return  none
 */
void WCHNET_DNSCallBack(const char *name, u8 *ipaddr, void *callback_arg)
{
    if (ipaddr == NULL)
    {
        printf("DNS Fail\r\n");
        return;
    }
    printf("Host Name = %s\r\n", name);
    printf("IP= %d.%d.%d.%d\r\n", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
    if (callback_arg != NULL)
    {
        printf("callback_arg = %02x\r\n", (*(u8 *)callback_arg));
    }
    WCHNET_DNSStop(); // stop DNS,and release socket
}

/*********************************************************************
 * @fn      WCHNET_DHCPCallBack
 *
 * @brief   DHCPCallBack
 *
 * @param   status - status returned by DHCP
 *          arg - Data returned by DHCP
 *
 * @return  DHCP status
 */
u8 WCHNET_DHCPCallBack(u8 status, void *arg)
{
    u8 *p;

    if (!status)
    {
        p = arg;
        printf("DHCP Success\r\n");
        memcpy(IPAddr, p, 4);
        memcpy(GWIPAddr, &p[4], 4);
        memcpy(IPMask, &p[8], 4);
        printf("IPAddr = %d.%d.%d.%d \r\n", (u16)IPAddr[0], (u16)IPAddr[1],
               (u16)IPAddr[2], (u16)IPAddr[3]);
        printf("GWIPAddr = %d.%d.%d.%d \r\n", (u16)GWIPAddr[0], (u16)GWIPAddr[1],
               (u16)GWIPAddr[2], (u16)GWIPAddr[3]);
        printf("IPMask = %d.%d.%d.%d \r\n", (u16)IPMask[0], (u16)IPMask[1],
               (u16)IPMask[2], (u16)IPMask[3]);
        printf("DNS1: %d.%d.%d.%d \r\n", p[12], p[13], p[14], p[15]); // DNS server address provided by the router
        printf("DNS2: %d.%d.%d.%d \r\n", p[16], p[17], p[18], p[19]);

        WCHNET_InitDNS(&p[12], DnsPort);                                        // Set DNS server IP address, and DNS server port is 53
        WCHNET_HostNameGetIp("www.wch.cn", RemoteIp, WCHNET_DNSCallBack, NULL); // Start DNS
        return READY;
    }
    else
    {
        printf("DHCP Fail %02x \r\n", status);
        return NoREADY;
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program
 *
 * @return  none
 */
extern u8 publishValid;

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef initLed = {0};
    initLed.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    initLed.GPIO_Pin = GPIO_Pin_3;
    initLed.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &initLed);

    u8 i;
    Delay_Init();
    USART_Printf_Init(115200); // USART initialize
    printf("MQTT Test\r\n");
    if ((SystemCoreClock == 60000000) || (SystemCoreClock == 120000000))
        printf("SystemClk:%d\r\n", SystemCoreClock);
    else
        printf("Error: Please choose 60MHz and 120MHz clock when using Ethernet!\r\n");
    printf("net version:%x\n", WCHNET_GetVer());
    if (WCHNET_LIB_VER != WCHNET_GetVer())
    {
        printf("version error.\n");
    }
    WCHNET_GetMacAddr(MACAddr); // get the chip MAC address
    printf("mac addr:");
    for (i = 0; i < 6; i++)
        printf("%x ", MACAddr[i]);
    printf("\n");

    TIM2_Init();
    
    
//  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
//     GPIO_InitTypeDef initLed = {0};
//     initLed.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//     initLed.GPIO_Pin = GPIO_Pin_3;
//     initLed.GPIO_Speed = GPIO_Speed_2MHz;
//     GPIO_Init(GPIOB, &initLed);
    WCHNET_DHCPSetHostname("WCHNET");
    i = ETH_LibInit(IPAddr, GWIPAddr, IPMask, MACAddr); // Ethernet library initialize
    mStopIfError(i);
    if (i == WCHNET_ERR_SUCCESS)
        printf("WCHNET_LibInit Success\r\n");
    WCHNET_DHCPStart(WCHNET_DHCPCallBack);
    Transport_Open(); // open the TCP connection.

    while (1)
    {
        /*Ethernet library main task function,
         * which needs to be called cyclically*/
        WCHNET_MainTask();
        /*Query the Ethernet global interrupt,
         * if there is an interrupt, call the global interrupt handler*/
        if (WCHNET_QueryGlobalInt())
        {
            WCHNET_HandleGlobalInt();
        }
        if (    publishValid == 1)
        {
            // printf("Button Pressed\r\n");
            publishValid = 0;
            if (con_flag)
                MQTT_Publish(pub_topic, pub_qos, payload);
            //    if(con_flag) MQTT_Pingreq();                                   //heartbeat packet
        }
    }
}
