#include "MQTTPacket.h"
#include "app_mqtt_handle.h"
#include "tcp_client.h"
#include "esp_log.h"
#include "esp_libc.h"

TaskHandle_t xHandleTasMQTT = NULL;                   // 任务句柄，删除任务用
static wifi_stauts_t wifi_status = WIFI_DISCONNECTED; // 创建局部变量
uint8_t buff_receive[RECV_BUFF_SIZE] = {0};           // socket通信 数据接收buff
uint8_t mqtt_pack[MQTT_PACK_SIZE] = {0};

static const char *TAG = "MQTT";
static const char *XMQTT_SELF_VERSION = "xMQTT_v1.0";

static xMQTT_CONFIG xMqttConfig;

//mqtt收包缓存变量(队列项)
static xMQTT_Msg rMqttMsg;
//mqtt发包缓存变量(队列项)
static xMQTT_Msg wMqttMsg;

static xQueueHandle mqttSendMsgQueue = NULL; //mqtt发送数据队列
static xQueueHandle mqttRcvMsgQueue = NULL;  //mqtt接收数据队列

static mqtt_transfer_t mqtt =
    {
        .Pack = mqtt_pack,
};

static transport_recv_t recv =
    {
        .buff = buff_receive,
        .buff_len = 0,
};

void xMqttConnectWifiNotify(wifi_stauts_t state)
{
    wifi_status = state;
}

static wifi_stauts_t wifi_status_get(void)
{
    return wifi_status;
}

int transport_getdata(uint8_t *buf, int len)
{
    int i;
    if ((recv.buff_len > 0) && (recv.buff_len >= len))
    {
        for (i = 0; i < len; i++)
        {
            *(buf + i) = *(recv.buff + i);
        }
        recv.buff_len -= len;
        memcpy(recv.buff, recv.buff + len, recv.buff_len);
    }
    else
    {
        len = 0;
    }
    return (int)len;
}

void mqtt_ping_request(void)
{
    memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
    uint16_t len = MQTTSerialize_pingreq(mqtt.Pack, MQTT_PACK_SIZE);
    tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
}

void xMqttPublicMsg(xMQTT_Msg *mqttMsg)
{

    
    if (xQueueSend(mqttSendMsgQueue, mqttMsg, 0) != pdTRUE)
    {
    }

    // unsigned short packetid = 0;
    // MQTTString topicName = MQTTString_initializer;
    // topicName.cstring = (char *)mqttMsg->topic;
    // memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
    // int len = MQTTSerialize_publish(mqtt.Pack, MQTT_PACK_SIZE, mqttMsg->dup, mqttMsg->qos, mqttMsg->retained, packetid, topicName, mqttMsg->payload, mqttMsg->payloadlen);
    // tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
}

void xMqttSubTopic(xMQTT_Msg *mqttMsg)
{
    MQTTString topicString = MQTTString_initializer;
    topicString.cstring = (char *)mqttMsg->topic;
    int32_t msgid = 1;
    int len;
    memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
    len = MQTTSerialize_subscribe(mqtt.Pack, MQTT_PACK_SIZE, 0, msgid, 1, &topicString, &mqttMsg->qos);
    tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
}

void mqtt_disconnect(void)
{
    memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
    uint16_t len = MQTTSerialize_disconnect(mqtt.Pack, MQTT_PACK_SIZE);
    tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
    close(connect_socket);
}

void TaskMainMqtt(void *pvParameters)
{

    ESP_LOGI(TAG, "MQTTVersion  = %d ", xMqttConfig.MQTTVersion);
    ESP_LOGI(TAG, "keepAliveInterval  = %d ", xMqttConfig.keepAliveInterval);
    ESP_LOGI(TAG, "cleansession  = %d ", xMqttConfig.cleansession);
    ESP_LOGI(TAG, "clientID = %s ", xMqttConfig.clientID);
    ESP_LOGI(TAG, "username  = %s ", xMqttConfig.username);
    ESP_LOGI(TAG, "password  = %s ", xMqttConfig.password);

    MQTTPacket_connectData pack = MQTTPacket_connectData_initializer;
    mqtt.pingCnt = 0; // 清零
    pack.MQTTVersion = xMqttConfig.MQTTVersion;
    pack.keepAliveInterval = xMqttConfig.keepAliveInterval;
    pack.cleansession = xMqttConfig.cleansession;
    pack.username.cstring = xMqttConfig.username;
    pack.password.cstring = xMqttConfig.password;
    pack.clientID.cstring = xMqttConfig.clientID;

    mqtt.LinkFlag = MQTT_DISCONNECTED; // 初始化

    uint8_t msgtypes = CONNECT;
    uint32_t curtick = xTaskGetTickCount();

    unsigned short submsgid;
    int subcount;
    int granted_qos;
    int len;
    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short packetid;
    MQTTString topicName = MQTTString_initializer;

    while (1)
    {

        if (wifi_status_get() == WIFI_CONNECTED)
        {

            if (xQueueReceive(mqttSendMsgQueue, &wMqttMsg, 0))
            {
                unsigned short packetid = 0;
                MQTTString topicName = MQTTString_initializer;
                topicName.cstring = (char *)wMqttMsg.topic;
                memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
                int len = MQTTSerialize_publish(mqtt.Pack, MQTT_PACK_SIZE, wMqttMsg.dup, wMqttMsg.qos, wMqttMsg.retained, packetid, topicName, wMqttMsg.payload, wMqttMsg.payloadlen);
                tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            }

            if ((xTaskGetTickCount() - curtick) > (pack.keepAliveInterval * 1000 / portTICK_RATE_MS))
            {

                if (mqtt.LinkFlag == MQTT_CONNECTED) // 已经连上
                {
                    curtick = xTaskGetTickCount();
                    mqtt_ping_request(); // 发送心跳包
                    mqtt.pingCnt++;

                    if (mqtt.pingCnt > 3) // 已经失去连接
                    {
                        ESP_LOGI(TAG, "MQTT lost link... \r\n");
                        mqtt_disconnect(); // 释放socket资源
                        mqtt.pingCnt = 0;
                        mqtt.LinkFlag = MQTT_DISCONNECTED;
                        msgtypes = CONNECT; // 重新开始新的连接
                        xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_NET_DISCONN);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "mqtt.pingCnt %d", mqtt.pingCnt);
                        xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_HEART_BEAT);
                    }
                }
            }
        }
        else
        {
            xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_NET_DISCONN);
            ESP_LOGI(TAG, "mqtt_handle_task wifi status fail...");
            mqtt_disconnect(); // 释放socket资源
            mqtt.pingCnt = 0;
            mqtt.LinkFlag = MQTT_DISCONNECTED;
            msgtypes = CONNECT; // 重新开始新的连接
        }

        //xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_MQTT_CONN_SUCC);
        switch (msgtypes)
        {

        //连接状态或者重连
        case CONNECT:
            //ESP_LOGI(TAG, "connectting borkerHost: %s : %d", xMqttConfig.borkerHost, xMqttConfig.borkerPort);
            xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_MQTT_CONNECTING);
            memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
            len = MQTTSerialize_connect(mqtt.Pack, MQTT_PACK_SIZE, &pack);
            if (mqtt_tcp_connect(xMqttConfig.borkerHost, xMqttConfig.borkerPort) != 0) // 连接MQTT服务器
            {
                mqtt.LinkFlag = MQTT_DISCONNECTED;
                vTaskDelay(1000 / portTICK_RATE_MS);
                msgtypes = CONNECT;
                break;
            }
            tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送

            //ESP_LOGI(TAG, "free heap size = %d\n", esp_get_free_heap_size());
            break;

        case CONNACK:
            ESP_LOGI(TAG, "MQTT server connect success\r\n");
            memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
            msgtypes = SUBSCRIBE; //
            mqtt.LinkFlag = MQTT_CONNECTED;
            xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_MQTT_CONN_SUCC);
            //复位队列
            xQueueReset(mqttSendMsgQueue);
            xQueueReset(mqttRcvMsgQueue);
            break;

        case PUBLISH:

            memset(&rMqttMsg, 0x0, sizeof(rMqttMsg));
            MQTTDeserialize_publish((unsigned char *)(&dup), (int *)(&qos), (unsigned char *)(&retained),
                                    (unsigned short *)(&packetid), (MQTTString *)(&topicName),
                                    &(mqtt.Data), &(mqtt.DataLen),
                                    mqtt.Pack, MQTT_PACK_SIZE);

            uint16_t payloadlen = mqtt.DataLen;
            uint16_t topicLen = topicName.lenstring.len;

            //strncpy(rMqttMsg.topic, topicName.topicName.lenstring.data, topicLen);
            memcpy(rMqttMsg.topic, topicName.lenstring.data, topicLen);
            rMqttMsg.topic[topicLen] = '\0';
            rMqttMsg.qos = qos;
            rMqttMsg.retained = retained;
            rMqttMsg.payloadlen = mqtt.DataLen;
            memcpy(rMqttMsg.payload, mqtt.Data, payloadlen);
            xQueueSend(mqttRcvMsgQueue, &rMqttMsg, 0);

            // ESP_LOGI(TAG, "Recv topicName: %s , payload: %s", rMqttMsg.topic, rMqttMsg.payload);
            // ESP_LOGI(TAG, "Recv topicName len: %d , payload len: %d", rMqttMsg.payloadlen, payloadlen);

            if (qos == 1) //
            {
                // ESP_LOGI(TAG, "publish qos is 1,send publish ack\r\n");
                memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
                len = MQTTSerialize_ack(mqtt.Pack, MQTT_PACK_SIZE, PUBACK, dup, packetid);
                tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            }
            else if (qos == 2)
            {
                memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
                len = MQTTSerialize_ack(mqtt.Pack, MQTT_PACK_SIZE, PUBREC, dup, packetid);
                tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            }
            msgtypes = 0;
            xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_RECEIVE_SUCC);
            break;

        case PUBACK:
            msgtypes = 0;
            break;

        case PUBREC: // just for qos2
        case PUBREL: // just for qos2
        {
            unsigned short packetid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &packetid, mqtt.Pack, MQTT_PACK_SIZE) != 1)
            {
                ESP_LOGI(TAG, "PUBREL Error\r\n");
            }
            else if (((len = MQTTSerialize_ack(mqtt.Pack, MQTT_PACK_SIZE, (msgtypes == PUBREC) ? PUBREL : PUBCOMP, 0, packetid)) != 0))
            {
                tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            }
            msgtypes = 0;
        }
        break;
        case PUBCOMP: // just for qos2
            msgtypes = 0;
            break;

        case SUBSCRIBE:
            ESP_LOGI(TAG, "start subTopic");
            // topicString.cstring = SUBTOPIC;
            // memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
            // len = MQTTSerialize_subscribe(mqtt.Pack, MQTT_PACK_SIZE, 0, msgid, 1, &topicString, &req_qos);
            // tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            msgtypes = 0;
            break;

        case SUBACK:
            MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, mqtt.Pack, MQTT_PACK_SIZE);
            msgtypes = 0;
            ESP_LOGI(TAG, "subscribe success\r\n");

            xMqttConfig.xmqtt_event_callback(XP_MQTT_EVENT_SUBMSG_SUCC);
            break;

        case PINGREQ:
            memset(mqtt.Pack, 0, MQTT_PACK_SIZE);
            len = MQTTSerialize_pingreq(mqtt.Pack, MQTT_PACK_SIZE);
            tcp_send(connect_socket, mqtt.Pack, len, 500); // 发送
            ESP_LOGI(TAG, "sending ping...\r\n");
            msgtypes = 0;
            break;

        case PINGRESP:
            msgtypes = 0;
            mqtt.pingCnt = 0; // 计数清零
            //ESP_LOGI(TAG, "Recv PINGRESP\r\n");
            break;
        }
        recv.buff_len = tcp_receive(connect_socket, recv.buff, RECV_BUFF_SIZE, 150); // 非阻塞

        if (recv.buff_len > 0)
        {
            // ESP_LOGI(TAG, "recv.buff_len = %d", recv.buff_len);
            msgtypes = (enum msgTypes)MQTTPacket_read(mqtt.Pack, MQTT_PACK_SIZE, transport_getdata);
            // ESP_LOGI(TAG, "mqtt_handle_task while(1) , free heap size = %d ", esp_get_free_heap_size());
            // ESP_LOGI(TAG, "msgType = %d ", msgtypes);
            // ESP_LOGI(TAG, "buff = %s ", recv.buff);
        }

        vTaskDelay(500 / portTICK_RATE_MS);
    }
    mqtt_disconnect();
}

xMQTT_ERR_CODE xMqttReceiveMsg(xMQTT_Msg *msg)
{
    if (mqttRcvMsgQueue == NULL)
    {
        printf("xMqttReceiveMsg fail \n");
        return 0;
    }
    return xQueueReceive(mqttRcvMsgQueue, msg, 10);
}

xMQTT_ERR_CODE xMqttInit(xMQTT_CONFIG *config)
{

    xMQTT_ERR_CODE err = XP_MQTT_SUCCESS;

    xMQTT_CONFIG mConfig;
    memset(&mConfig, 0, sizeof(xMQTT_CONFIG));

    mConfig.MQTTVersion = config->MQTTVersion;
    mConfig.keepAliveInterval = config->keepAliveInterval;
    mConfig.cleansession = config->cleansession;
    mConfig.borkerPort = config->borkerPort;
    mConfig.mqttCommandTimeout = config->mqttCommandTimeout;

    mConfig.borkerHost = (char *)os_zalloc(strlen(config->borkerHost) + 1);
    mConfig.username = (char *)os_zalloc(strlen(config->username) + 1);
    mConfig.password = (char *)os_zalloc(strlen(config->password) + 1);
    mConfig.clientID = (char *)os_zalloc(strlen(config->clientID) + 1);

    mConfig.xmqtt_event_callback = config->xmqtt_event_callback;

    if (mConfig.borkerHost == NULL ||
        mConfig.username == NULL ||
        mConfig.password == NULL ||
        mConfig.clientID == NULL)
    {
        err = XP_MQTT_ERR_MALLOC_FAIL;
        goto __FINISH;
    }
    else
    {
        strcpy(mConfig.borkerHost, config->borkerHost);
        strcpy(mConfig.username, config->username);
        strcpy(mConfig.password, config->password);
        strcpy(mConfig.clientID, config->clientID);
    }

    mqttRcvMsgQueue = xQueueCreate(XP_MQTT_RCV_MSG_QUEUE_LEN, sizeof(xMQTT_Msg));
    mqttSendMsgQueue = xQueueCreate(XP_MQTT_SEND_MSG_QUEUE_LEN, sizeof(xMQTT_Msg));

    if (mqttRcvMsgQueue == NULL)
    {
        err = XP_MQTT_ERR_MALLOC_FAIL;
        goto __FINISH;
    }

    xMqttConfig = mConfig;

__FINISH:
    if (err != XP_MQTT_SUCCESS)
    {
        free(mConfig.borkerHost);
        free(mConfig.username);
        free(mConfig.password);
        free(mConfig.clientID);
        vQueueDelete(mqttRcvMsgQueue);
        vQueueDelete(mqttSendMsgQueue);
        mqttRcvMsgQueue = NULL;
        mqttSendMsgQueue = NULL;
        memset(&mConfig, 0, sizeof(xMQTT_CONFIG));
        printf("XP_MQTT_START fail(%d) \n", err);
    }
    return err;
}

char *getXMqttVersion()
{
    return XMQTT_SELF_VERSION;
}