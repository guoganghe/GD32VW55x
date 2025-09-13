/*!
    \file    atcmd_tcpip.c
    \brief   AT command TCPIP part for GD32VW55x SDK

    \version 2023-07-20, V1.0.0, firmware for GD32VW55x
*/

/*
    Copyright (c) 2023, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "slist.h"

static cip_info_t cip_info;
static int cip_task_started = 0;
static int cip_task_terminate = 0;

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

typedef struct _passth_tx_buf {
    char *buf;
    uint32_t size;
    uint32_t writeptr;
    uint32_t readptr;
} passth_tx_buf_t;

typedef struct _cip_passth_info {
    int passth_fd_idx;
    passth_tx_buf_t passth_buf;

    os_timer_t passth_timer;
    volatile uint8_t at_tx_passth_timeout;
    volatile uint8_t terminate_send_passth;
} cip_passth_info_t;
static cip_passth_info_t cip_passth_info;

int local_sock_send = -1;

#ifdef CONFIG_ATCMD_SPI
typedef struct _cip_file_transfer_info {
    int fd_idx;
    uint32_t file_len;
    uint32_t segment_len;
    uint32_t remaining_len;
    uint32_t cur_len; // current processed len
    uint8_t *s_buf;

    volatile uint8_t terminate;
} cip_file_transfer_info_t;
static cip_file_transfer_info_t cip_file_trans_info;
const char *ack = "ACK";
const char *nak = "NAK";
#endif

/*!
    \brief      initialize structure of tcpip information
    \param[in]  none
    \param[out] none
    \retval     none
*/
void cip_info_init(void)
{
    int i;

    cip_task_terminate = 0;

    sys_memset(&cip_info, 0, sizeof(cip_info));
    cip_info.trans_intvl = CIP_TRANSFER_INTERVAL_DEFAULT; //ms
    cip_info.local_srv_fd = -1;
    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        cip_info.cli[i].fd = -1;
    }
    return;
}

/*!
    \brief      allocate storage space for tcpip information
    \param[in]  none
    \param[out] none
    \retval     the location of the array used to store tcpip information
*/
static int cip_info_cli_alloc(void)

{
    int i;

    if (cip_info.cli_num >= MAX_CLIENT_NUM) {
        return -1;
    }
    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd < 0)
            return i;
    }
    return -1;
}

/*!
    \brief      store tcpip information
    \param[in]  fd: the ID of the array used to store tcpip information
    \param[in]  type: the type of the client
    \param[in]  role: the role of the client
    \param[in]  remote_ip: remote ip
    \param[in]  remote_port: remote port
    \param[in]  local_port: local port
    \param[out] none
    \retval     the location of the array used to store tcpip information
*/
static int cip_info_cli_store(int fd, char *type, uint8_t role,
                            uint32_t remote_ip, uint16_t remote_port, uint16_t local_port)
{
    int idx = cip_info_cli_alloc();

    if ((idx < 0) || (fd < 0))
        return -1;
    cip_info.cli[idx].fd = fd;
    if (strncmp(type, "TCP", 3) == 0)
        cip_info.cli[idx].type = CIP_TYPE_TCP;
    else
        cip_info.cli[idx].type = CIP_TYPE_UDP;
    cip_info.cli[idx].role = role;
    cip_info.cli[idx].stop_flag = 0;
    cip_info.cli[idx].remote_ip = remote_ip;
    cip_info.cli[idx].remote_port = remote_port;
    cip_info.cli[idx].local_port = local_port;

    cip_info.cli_num++;

    return idx;
}

/*!
    \brief      free the specified array that stores tcpip information
    \param[in]  index: the location of the array used to store tcpip information
    \param[out] none
    \retval     none
*/
static void cip_info_cli_free(int index)
{
    if ((index >= 0) && (index < MAX_CLIENT_NUM)) {
        if (cip_info.cli[index].fd != -1) {
#ifdef CONFIG_ATCMD_SPI
            struct recv_data_node *p_item;
            sys_mutex_get(&cip_info.cli[index].list_lock);
            p_item = (struct recv_data_node *)list_pick(&cip_info.cli[index].recv_data_list);

            while (p_item != NULL) {
                if (p_item->data && (p_item->data_len > 0)) {
                    sys_mfree(p_item->data);
                    sys_mfree(p_item);
                }
                list_remove(&cip_info.cli[index].recv_data_list, NULL, (struct list_hdr *)p_item);
                p_item = (struct recv_data_node *)list_pick(&cip_info.cli[index].recv_data_list);
            }
            sys_mutex_free(&cip_info.cli[index].list_lock);
#endif
            sys_memset(&cip_info.cli[index], 0, sizeof(cip_info.cli[index]));
            cip_info.cli[index].fd = -1;
            cip_info.cli_num--;
        }
    }
}

/*!
    \brief      find the specified array that stores tcpip information
    \param[in]  fd: the ID of the array used to store tcpip information
    \param[out] none
    \retval     location of the array used to store tcpip information
*/
static int cip_info_cli_find(int fd)

{
    int i;

    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd == fd)
            return i;
    }
    return -1;
}

static int cip_info_valid_fd_cnt_get(void)
{
    int i, cnt = 0;

    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd >= 0)
            cnt++;
    }
    return cnt;
}

static int cip_info_valid_tcp_fd_cnt_get(void)
{
    int i, cnt = 0;

    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd >= 0 && cip_info.cli[i].type == CIP_TYPE_TCP)
            cnt++;
    }
    return cnt;
}


/*!
    \brief      reset tcpip information
    \param[in]  none
    \param[out] none
    \retval     none
*/
void cip_info_reset(void)
{
    int i, fd;

    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd >= 0) {
            fd = cip_info.cli[i].fd;
            cip_info_cli_free(i);
            close(fd);
        }
    }
    if (cip_info.local_srv_fd >= 0) {
        fd = cip_info.local_srv_fd;
        cip_info.local_srv_fd = -1;
        cip_info.local_srv_port = 0;
        close(fd);
    }
    cip_task_terminate = 1;
}


/*!
    \brief      start a tcp client
    \param[in]  srv_ip: server IP address
    \param[in]  srv_port: server port
    \param[in]  bkeep_alive: time of keep alive
    \param[out] none
    \retval     the result of operation
*/
static int tcp_client_start(char *srv_ip, uint16_t srv_port, uint8_t bkeep_alive, int *fd_ptr)
{
    struct sockaddr_in saddr;
    socklen_t len = sizeof(saddr);
    uint32_t nodelay = 0, keep_alive = 10; /* Seconds */
    int fd, ret, idx;
    uint32_t srv_ip_int = inet_addr(srv_ip);

#ifndef CONFIG_ATCMD_SPI
    if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
        (cip_info_valid_fd_cnt_get() > 0 || cip_info.local_srv_fd >= 0))
        return -1;
#endif

    sys_memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(srv_port);
    saddr.sin_addr.s_addr = srv_ip_int;

    /* creating a TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        AT_TRACE("Create tcp client socket fd error!\r\n");
        return -1;
    }
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
            (const char *) &nodelay, sizeof( nodelay ) );
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE,
            (const char *) &keep_alive, sizeof( keep_alive ) );

    AT_TRACE("TCP: server IP=%s port=%d.\r\n", srv_ip, srv_port);

    /* connecting to TCP server */
    ret = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) {
        AT_TRACE("Tcp client connect server error!\r\n");
        ret = -2;
        goto Exit;
    }
    /* Get local port */
    sys_memset(&saddr, 0, sizeof(saddr));
    getsockname(fd, (struct sockaddr *)&saddr, &len);
    /* save client info */
    idx = cip_info_cli_store(fd, "TCP", CIP_ROLE_CLIENT,
                            srv_ip_int, srv_port, ntohs(saddr.sin_port));
    if (idx < 0) {
        AT_TRACE("Client num reached the maximum!\r\n");
        ret = -3;
        goto Exit;
    }
    AT_TRACE("TCP: create socket %d.\r\n", fd);
    cip_passth_info.passth_fd_idx = idx;
    *fd_ptr = fd;

    return 0;

Exit:
    close(fd);
    return ret;
}

/*!
    \brief      send tcp packet
    \param[in]  fd: the socket of the tcp client
    \param[in]  tx_len: length of tcp packet to be sent
    \param[out] none
    \retval     the result of operation
*/
static int at_tcp_send(int fd, uint32_t tx_len)
{
    char *tx_buf = NULL;
    int cnt = 0;
    int retry_cnt = 10;
    struct at_local_tcp_send send_data;

    tx_buf = sys_zalloc(tx_len);
    if (NULL == tx_buf) {
        AT_TRACE("Allocate client buffer failed (len = %u).\r\n", tx_len);
        return -1;
    }

    AT_RSP_DIRECT(">\r\n", 3);

    // Block here to wait dma receive done
    at_hw_dma_receive((uint32_t)tx_buf, tx_len);
    send_data.event_id = AT_LOCAL_TCP_SNED_EVENT;
    send_data.sock_fd = fd;
    send_data.send_data_addr = (uint32_t)tx_buf;
    send_data.send_data_len = tx_len;

Retry:
    cnt = sendto(local_sock_send, (void *)&send_data, sizeof(send_data), 0, NULL, 0);
    if (cnt <= 0) {
        if ((errno == EAGAIN || errno == ENOMEM) && retry_cnt > 0) {
            sys_ms_sleep(20);
            retry_cnt--;
            goto Retry;
        }
        sys_mfree(tx_buf);
        AT_TRACE("local socket send tcp fail. %d!\r\n", errno);
        AT_RSP_START(10);
        AT_RSP("SEND FAIL\r\n");
        AT_RSP_IMMEDIATE();
        AT_RSP_FREE();
    }
    return cnt;
}

#ifndef CONFIG_ATCMD_SPI
static void cip_passth_tx_buf_deinit()
{
    if (cip_passth_info.passth_buf.buf)
        sys_mfree(cip_passth_info.passth_buf.buf);

    cip_passth_info.passth_buf.buf= NULL;
    cip_passth_info.passth_buf.size = 0;

    cip_passth_info.passth_buf.writeptr = 0;
    cip_passth_info.passth_buf.readptr = 0;
}

static int cip_passth_tx_buf_init()
{
    if (cip_passth_info.passth_buf.buf == NULL) {
        cip_passth_info.passth_buf.buf = (char *)sys_zalloc(PASSTH_TX_BUF_LEN);
        if (cip_passth_info.passth_buf.buf == NULL)
            return -1;
    }

    cip_passth_info.passth_buf.size = PASSTH_TX_BUF_LEN;
    cip_passth_info.passth_buf.writeptr = 0;
    cip_passth_info.passth_buf.readptr = 0;

    return 0;
}

static void cip_passth_info_deinit(void)
{
    if (cip_passth_info.passth_timer) {
        sys_timer_delete(&(cip_passth_info.passth_timer));
    }

    cip_passth_info.terminate_send_passth = 0;
    cip_passth_info.at_tx_passth_timeout = 0;

    cip_passth_tx_buf_deinit();
}

static int cip_passth_info_init(void)
{
    cip_passth_info.terminate_send_passth = 0;
    cip_passth_info.at_tx_passth_timeout = 0;

    if (cip_passth_tx_buf_init() < 0) {
        goto fail;
    }

    return 0;

fail:
    cip_passth_info_deinit();
    return -1;
}

static int at_passth_send_data(int fd, uint8_t flush, uint8_t type)
{
    passth_tx_buf_t *passth_tx_buf = &(cip_passth_info.passth_buf);
    char *start_addr = passth_tx_buf->buf + passth_tx_buf->readptr;
    int ret = 0, sent_cnt = 0, idx = 0;
    int retry_cnt = 0;
    struct sockaddr_in saddr;
    int remaining_cnt = passth_tx_buf->writeptr - passth_tx_buf->readptr;

    if (fd < 0 || ((type != CIP_TYPE_TCP) && (type != CIP_TYPE_UDP)))
        return -1;

    if (remaining_cnt == 0)
        return 0;

    if (type == CIP_TYPE_UDP) {
        idx = cip_info_cli_find(fd);
        if (idx == -1)
            return -1;
        sys_memset(&saddr, 0, sizeof(struct sockaddr_in));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(cip_info.cli[idx].remote_port);
        saddr.sin_addr.s_addr = cip_info.cli[idx].remote_ip;
    }

    if ((remaining_cnt == strlen(PASSTH_TERMINATE_STR) &&
            strncmp(passth_tx_buf->buf, PASSTH_TERMINATE_STR, strlen(PASSTH_TERMINATE_STR)) == 0)
        //|| (remaining_cnt == 5 && strncmp(passth_tx_buf->buf, PASSTH_TERMINATE_STR"\r\n", 5) == 0)
        ) {
        cip_passth_info.terminate_send_passth = 1;
        return 0;
    }

    while (remaining_cnt > 0) {
        if (remaining_cnt >= PASSTH_START_TRANSFER_LEN) {
            sent_cnt = PASSTH_START_TRANSFER_LEN;
        } else {
            if (flush == 1)
                sent_cnt = remaining_cnt;
            else
                return 0;
        }

Retry:
        if (type == CIP_TYPE_TCP)
            ret = send(fd, (void *)start_addr, sent_cnt, 0);
        else
            ret = sendto(fd, (void *)start_addr, sent_cnt, 0, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

        if (ret <= 0) {
            if (errno == EAGAIN || errno == ENOMEM) {
                if (retry_cnt > 0) {
                    retry_cnt--;
                    goto Retry;
                }
            }
            AT_TRACE("send error:%d\r\n", errno);
            goto exit;
        }

        passth_tx_buf->readptr += sent_cnt;
        // AT_TRACE("Sendout: %d OK,w:%d,r:%d,s:%d,f=%d,t=%d\r\n", ret, passth_tx_buf->writeptr, passth_tx_buf->readptr, sent_cnt, flush, cip_passth_info.at_tx_passth_timeout);
        start_addr += sent_cnt;
        remaining_cnt = remaining_cnt - sent_cnt;
    }

    return 0;
exit:
    cip_passth_info.terminate_send_passth = 1;
    cip_passth_info.passth_fd_idx = -1;
    return -1;
}

static void at_tx_passth_timeout_cb( void *ptmr, void *p_arg )
{
    cip_passth_info.at_tx_passth_timeout = 1;
}

static int at_hw_passth_send(int fd, uint8_t type)
{
    char *tx_buf = NULL;
    passth_tx_buf_t *passth_tx_buf;
    int passth_timeout = 0, ret;
    uint32_t last_cnt = 0, cur_cnt = 0;

    if (cip_passth_info_init()) {
        AT_RSP_DIRECT("ERROR\r\n", 7);
        return -1;
    }

    passth_tx_buf = &(cip_passth_info.passth_buf);

    if (cip_info.trans_intvl == 0)
        passth_timeout = 1;
    else
        passth_timeout = cip_info.trans_intvl;

    sys_timer_init(&(cip_passth_info.passth_timer), (const uint8_t *)("passth_intvl_timer"),
            passth_timeout, 0, at_tx_passth_timeout_cb, NULL);

    at_hw_dma_receive_config();

Loop:
    tx_buf = passth_tx_buf->buf;
    passth_tx_buf->writeptr = 0;
    passth_tx_buf->readptr = 0;

    last_cnt = 0;
    cur_cnt = 0;
    cip_passth_info.at_tx_passth_timeout = 0;

    at_hw_dma_receive_start((uint32_t)tx_buf, passth_tx_buf->size);
    sys_timer_start(&(cip_passth_info.passth_timer), 0);

    while (cip_passth_info.terminate_send_passth != 1) {
        ret = sys_sema_down(&at_hw_dma_sema, 1); // wait 1ms
        cur_cnt = at_dma_get_cur_received_num(cip_passth_info.passth_buf.size);
        if (ret == OS_OK) {  //8192 received
            // clear timer
            sys_timer_stop(&(cip_passth_info.passth_timer), 0);
            passth_tx_buf->writeptr = passth_tx_buf->size;
            //AT_TRACE("MAX, w:%d, r:%d, c=%d\r\n", passth_tx_buf->writeptr, passth_tx_buf->readptr, cur_cnt);
            at_passth_send_data(fd, 1, type);
            goto Loop;
        } else if (ret == OS_TIMEOUT && cip_passth_info.at_tx_passth_timeout != 1) {
            passth_tx_buf->writeptr = cur_cnt;
            //AT_TRACE("Wait, w:%d, r:%d, c=%d\r\n", passth_tx_buf->writeptr, passth_tx_buf->readptr, cur_cnt);
            if (passth_tx_buf->writeptr - passth_tx_buf->readptr >= PASSTH_START_TRANSFER_LEN) {
                at_passth_send_data(fd, 0, type);
            }
            continue;
        }

        if (cip_passth_info.at_tx_passth_timeout == 1 && cur_cnt > 0) {
            at_hw_dma_receive_stop();
            //AT_TRACE("Timeout, w:%d, r:%d, c=%d\r\n", passth_tx_buf->writeptr, passth_tx_buf->readptr, cur_cnt);
            passth_tx_buf->writeptr = cur_cnt;
            if (passth_tx_buf->writeptr > passth_tx_buf->readptr)
                at_passth_send_data(fd, 1, type);
            goto Loop;
        }

        cip_passth_info.at_tx_passth_timeout = 0;
        sys_timer_start(&(cip_passth_info.passth_timer), 0); // Restart timer

        //AT_TRACE("end, l:%d, c=%d, ret=%d\r\n", last_cnt, cur_cnt, ret);
        last_cnt = cur_cnt;
    }

//    AT_TRACE("PassThrough mode exit...\r\n");
    at_hw_dma_receive_stop();
    at_hw_irq_receive_config();
    cip_passth_info_deinit();
    return 0;
}
#endif


#ifdef CONFIG_ATCMD_SPI
static int cip_file_transfer_info_init(int idx, uint32_t file_len, uint32_t segment_len)
{
    uint8_t *tx_buf;

    if (idx < 0 || file_len == 0 || segment_len == 0) {
        return -1;
    }

    tx_buf = sys_malloc(segment_len + FILE_SEGMENT_CRC_LEN);
    if (tx_buf == NULL) {
        return -2;
    }
    sys_memset((void *)&cip_file_trans_info, 0, sizeof(cip_file_trans_info));
    cip_file_trans_info.fd_idx = idx;
    cip_file_trans_info.file_len = file_len;
    cip_file_trans_info.segment_len = segment_len;
    cip_file_trans_info.remaining_len = file_len;
    cip_file_trans_info.s_buf = tx_buf;

    return 0;
}

static void cip_file_transfer_info_deinit(void)
{
    if (cip_file_trans_info.s_buf)
        sys_mfree(cip_file_trans_info.s_buf);

    sys_memset((void *)&cip_file_trans_info, 0, sizeof(cip_file_trans_info));
    cip_file_trans_info.fd_idx = -1;
    cip_file_trans_info.terminate = 1;
}

static int at_file_send_data(int fd_idx, uint8_t *tx_buf, int tx_len)
{
    int fd, type;
    int ret = 0, sent_cnt = 0, retry_cnt = 3;
    struct sockaddr_in saddr;

    if (fd_idx < 0 || tx_len <= 0)
        return -1;


    fd = cip_info_cli_find(fd_idx);
    if (fd == -1)
        return -1;

    type = cip_info.cli[fd_idx].type;
    if (type == CIP_TYPE_UDP) {
        sys_memset(&saddr, 0, sizeof(struct sockaddr_in));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(cip_info.cli[fd_idx].remote_port);
        saddr.sin_addr.s_addr = cip_info.cli[fd_idx].remote_ip;
    }

Retry:
    if (type == CIP_TYPE_TCP)
        ret = send(fd, (void *)tx_buf, tx_len, 0);
    else
        ret = sendto(fd, (void *)tx_buf, tx_len, 0, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

    if (ret <= 0) {
        if (errno == EAGAIN || errno == ENOMEM) {
            if (retry_cnt > 0) {
                retry_cnt--;
                goto Retry;
            }
        }
        //AT_TRACE("send to fail, ret=%d\r\n", ret);
    }

    return ret;
}

static int at_send_file(int fd_idx, uint32_t file_len, uint32_t segment_len)
{
    uint8_t *tx_buf = cip_file_trans_info.s_buf;
    uint32_t remaining_len = file_len, real_len, checksum, len_align;
    __IO uint32_t read_data;
    int loop = file_len/segment_len + 1;
    int remain;

    rcu_periph_clock_enable(RCU_CRC);

    while(remaining_len > 0 && (cip_file_trans_info.terminate == 0)) {
        real_len = min(segment_len, remaining_len);
        AT_TRACE("Waiting the %dth data\r\n", loop);

        spi_manager.stat = SPI_Slave_File_Recv;
        spi_manager.direction = SPI_Slave_RX_Dir;
        at_hw_dma_receive((uint32_t)tx_buf, real_len + FILE_SEGMENT_CRC_LEN);
        loop--;

        remain = real_len & 0x03;
        len_align = real_len - remain;
        crc_data_register_reset();
        checksum = crc_block_data_calculate((uint32_t *)tx_buf, real_len / 4);
        if (remain) {
            read_data = *(uint32_t *)(tx_buf + len_align);
            read_data = ((read_data  << (8 * (4 - remain))) >> (8 * (4 - remain)));
            checksum = crc_single_data_calculate(read_data);
        }
//        spi_manager.stat = SPI_Slave_File_ACK;

        if (checksum == *(uint32_t *)(tx_buf + real_len)) {
            AT_TRACE("CRC Verify OK, %dth\r\n", loop);
            at_file_send_data(fd_idx, tx_buf, real_len);
            if (remaining_len == real_len)
                spi_manager.stat = SPI_Slave_File_Done;
            at_hw_send((char *)ack, 3);
        } else {
            AT_TRACE("CRC Verify fail,  checksum=0x%x vs 0x%x\r\n",
                    checksum, *(uint32_t *)(tx_buf + real_len));
            at_hw_send((char *)nak, 3);
            continue;
        }
        AT_TRACE("Done, %d\r\n", loop);
        sys_memset(tx_buf, 0, segment_len + FILE_SEGMENT_CRC_LEN);
        remaining_len -= real_len;
    }

    cip_file_trans_info.terminate = 1;

    AT_TRACE("File Transfer Complete...\r\n");
//    at_hw_dma_receive_stop();
//    at_hw_irq_receive_config();

    cip_file_transfer_info_deinit();
    rcu_periph_clock_disable(RCU_CRC);

    return 0;
}
#endif
/*!
    \brief      start a udp client
    \param[in]  srv_ip: server IP address
    \param[in]  srv_port: server port
    \param[out] none
    \retval     the result of operation
*/
static int udp_client_start(char *srv_ip, uint16_t srv_port, uint16_t local_port, int *fd_ptr)
{
    struct sockaddr_in saddr;
    socklen_t len = sizeof(saddr);
    int reuse = 1;
    int fd, ret;

#ifndef CONFIG_ATCMD_SPI
    if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
            (cip_info_valid_fd_cnt_get() > 0 || cip_info.local_srv_fd >= 0))
        return -1;
#endif

    /* creating a UDP socket */
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        AT_TRACE("Create udp client socket fd error!\r\n");
        return -1;
    }
    setsockopt(fd , SOL_SOCKET, SO_REUSEADDR,
            (const char *)&reuse, sizeof(reuse));

    sys_memset((char *)&saddr, 0, sizeof(saddr));
    saddr.sin_family      = AF_INET;
    saddr.sin_len         = sizeof(saddr);

    saddr.sin_port        = htons(local_port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* binding the UDP socket to a random port */
    ret = bind(fd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) {
        AT_TRACE("Bind udp server socket fd error!\r\n");
        goto Exit;
    }
    /* Get local port */
    sys_memset(&saddr, 0, sizeof(saddr));
    getsockname(fd, (struct sockaddr *)&saddr, &len);
    //AT_TRACE("UDP local port %d\r\n", ntohs(saddr.sin_port));
    /* save client info */
    ret = cip_info_cli_store(fd, "UDP", CIP_ROLE_CLIENT,
                            inet_addr(srv_ip), srv_port, ntohs(saddr.sin_port));
    if (ret < 0) {
        AT_TRACE("Client num reached the maximum!\r\n");
        ret = -2;
        goto Exit;
    }
    AT_TRACE("UDP: create socket %d.\r\n", fd);

    if (local_port > 0)
        cip_passth_info.passth_fd_idx = ret; // local port is specified
    else
        cip_passth_info.passth_fd_idx = -1;
    *fd_ptr = fd;

    return 0;

Exit:
    close(fd);
    return ret;
}

/*!
    \brief      send udp packet
    \param[in]  fd: the socket of the udp client
    \param[in]  tx_len: length of tcp packet to be sent
    \param[in]  srv_ip: server ip
    \param[in]  srv_port: server port
    \param[out] none
    \retval     the result of operation
*/
static int at_udp_send(int fd, uint32_t tx_len, char *srv_ip, uint16_t srv_port)
{
    char *tx_buf = NULL;
//    char ch;
    int cnt = 0;
    int retry_cnt = 10;
    struct sockaddr_in saddr;
    struct at_local_udp_send send_data;

    tx_buf = sys_malloc(tx_len);
    if (NULL == tx_buf) {
        AT_TRACE("Allocate client buffer failed (len = %u).\r\n", tx_len);
        return -1;
    }
    AT_RSP_DIRECT(">\r\n", 3);
#if 0
    usart_interrupt_disable(at_uart_conf.usart_periph, USART_INT_RBNE);
    sys_priority_set(sys_current_task_handle_get(), MGMT_TASK_PRIORITY);
    while (1) {
        while (RESET == usart_flag_get(at_uart_conf.usart_periph, USART_FLAG_RBNE));
        ch = usart_data_receive(at_uart_conf.usart_periph);
        if ((ch == 0x0a || ch == 0x0d) && (cnt >= tx_len))
            break;
        if (cnt < tx_len) {
            tx_buf[cnt] = ch;
            cnt++;
        } else {
            AT_RSP("%c", ch);
        }
    }
    sys_priority_set(sys_current_task_handle_get(), CLI_PRIORITY);
    usart_interrupt_enable(at_uart_conf.usart_periph, USART_INT_RBNE);
    //buffer_dump("TX:", (uint8_t *)tx_buf, cnt);
#else
    // Block here to wait dma receive done
    at_hw_dma_receive((uint32_t)tx_buf, tx_len);
    send_data.event_id = AT_LOCAL_UDP_SNED_EVENT;
    send_data.sock_fd = fd;
    send_data.send_data_addr = (uint32_t)tx_buf;
    send_data.send_data_len = tx_len;

    // debug_print_dump_data("TX:", (char *)tx_buf, tx_len);
#endif

    sys_memset(&saddr, 0, sizeof(struct sockaddr_in));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(srv_port);
    saddr.sin_addr.s_addr = inet_addr(srv_ip);

    sys_memcpy(&(send_data.to), &saddr, sizeof(struct sockaddr_in));
    send_data.tolen = sizeof(struct sockaddr_in);

Retry:
    cnt = sendto(local_sock_send, (void *)&send_data, sizeof(send_data), 0, NULL, 0);
    if (cnt <= 0) {
        if ((errno == EAGAIN || errno == ENOMEM) && retry_cnt > 0) {
            sys_ms_sleep(20);
            retry_cnt--;
            goto Retry;
        }
        sys_mfree(tx_buf);
        AT_TRACE("local socket send udp fail. %d!\r\n", errno);
        AT_RSP_START(10);
        AT_RSP("SEND FAIL\r\n");
        AT_RSP_IMMEDIATE();
        AT_RSP_FREE();
    }
    return cnt;
}

/*!
    \brief      start a tcp server
    \param[in]  srv_port: server port
    \param[out] none
    \retval     the result of operation
*/
static int tcp_server_start(uint16_t srv_port)
{
    struct sockaddr_in s_local_addr;
    int status, reuse;
    int srv_fd;

    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        return -1;
    }

    AT_TRACE("Create TCP server socket %d.\r\n", srv_fd);
    reuse = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR,
            (const char *) &reuse, sizeof(reuse) );

    sys_memset((char *)&s_local_addr, 0, sizeof(s_local_addr));
    s_local_addr.sin_family      = AF_INET;
    s_local_addr.sin_len         = sizeof(s_local_addr);
    s_local_addr.sin_port        = htons(srv_port);
    s_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* binding the TCP socket to the TCP server address */
    status = bind(srv_fd, (struct sockaddr *)&s_local_addr, sizeof(s_local_addr));
    if( status < 0 ) {
        AT_TRACE("Bind tcp server socket fd error!\r\n");
        goto Exit;
    }
    AT_TRACE("Bind successfully.\r\n");

    /* putting the socket for listening to the incoming TCP connection */
    status = listen(srv_fd, 20);
    if( status != 0 ) {
        AT_TRACE("Listen tcp server socket fd error!\r\n");
        goto Exit;
    }
    cip_info.local_srv_fd = srv_fd;
    cip_info.local_srv_port = srv_port;
    cip_info.local_srv_stop = 0;
    AT_TRACE("TCP listen port %d\r\n", srv_port);

    return 0;

Exit:
    /* close the listening socket */
    close(srv_fd);
    return status;
}

/*!
    \brief      stop all tcp server
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void tcp_server_stop(void)
{
    int i, fd;
    int active_sock_num = 0;

    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if ((cip_info.cli[i].fd > -1) && (cip_info.cli[i].role == CIP_ROLE_CLIENT)) {
            active_sock_num++;
        }
        if (active_sock_num) {
            break;
        }
    }

    if (cip_info.local_srv_fd >= 0) {
        if (active_sock_num) {
            cip_info.local_srv_stop = 1;
        } else {
            cip_task_terminate = 1;
            while (sys_task_exist((const uint8_t *)"Cip Rcv")) {
                sys_ms_sleep(1);
            }
            cip_task_started = 0;
        }
    }
}

#ifdef CONFIG_ATCMD_SPI
/*!
    \brief      process recieved data from socket server and
                list the data in the client_info_t.
    \param[in]  idx:    index of which client in cip_info
    \param[in]  rx_buf: buffer that stored recieved data
    \param[in]  recv_sz: recieved data length
    \param[out] none
    \retval     none
*/
static void at_spi_recv_data_process(int idx, uint8_t *rx_buf, int recv_sz)
{
    int recv_processed = 0, currentdatasize = 0;
    struct recv_data_node *recv_data_node = NULL;
    uint8_t *data_recv = NULL;

    if (recv_sz > AT_SPI_MAX_DATA_LEN) {
        AT_TRACE("recv_sz:%d large than 2048.\r\n", recv_sz);
    }

    recv_processed = 0;
    do {
        recv_data_node = sys_malloc(sizeof(struct recv_data_node));
        currentdatasize = (recv_sz - recv_processed) > AT_SPI_MAX_DATA_LEN ? AT_SPI_MAX_DATA_LEN : (recv_sz - recv_processed);

        if (recv_data_node == NULL) {
            AT_TRACE("Allocate recv_data_node failed (len = %u).\r\n", sizeof(struct recv_data_node));
            break;
        }

        data_recv = sys_malloc(currentdatasize);// for data
        if (data_recv == NULL) {
            AT_TRACE("Allocate data_recv failed (len = %u).\r\n", currentdatasize);
            break;
        }

        sys_memcpy(data_recv, rx_buf + recv_processed, currentdatasize);// copy payload

        recv_data_node->data = data_recv;
        recv_data_node->data_len = currentdatasize;
        // AT_TRACE("idx:%d, list add,len:%d, fd:%d\r\n", idx, currentdatasize, cip_info.cli[i].fd);
        sys_mutex_get(&cip_info.cli[idx].list_lock);
        // if data number in list is large than MAX_RECV_DATA_NUM_IN_LIST, delete the first one in the list
        if (list_cnt(&cip_info.cli[idx].recv_data_list) > MAX_RECV_DATA_NUM_IN_LIST) {
            struct recv_data_node *p_item;
            AT_TRACE("data num in list is large than %d, delete the first one\r\n", MAX_RECV_DATA_NUM_IN_LIST);
            p_item = (struct recv_data_node *)list_pop_front(&cip_info.cli[idx].recv_data_list);
            sys_mfree(p_item->data);
            sys_mfree(p_item);
        }
        list_push_back(&cip_info.cli[idx].recv_data_list, &recv_data_node->list_hdr);
        sys_mutex_put(&cip_info.cli[idx].list_lock);

        recv_processed += currentdatasize;
    } while (recv_processed < recv_sz);

    return;
}
#endif /* CONFIG_ATCMD_SPI */

extern int dhcpd_ipaddr_is_valid(uint32_t ipaddr);
/*!
    \brief      receive task
    \param[in]  param: the pointer of user parameter
    \param[out] none
    \retval     none
*/
static void cip_recv_task(void *param)
{
    struct timeval timeout;
    int max_fd_num = 0;
    int cli_fd, i, j, recv_sz;
    char *rx_buf;
    uint32_t rx_len = PASSTH_START_TRANSFER_LEN;
    struct sockaddr_in saddr;
    int addr_sz = sizeof(saddr);
    fd_set read_set, except_set;
    int status;
    int keepalive = 1;
    int keepidle = 20; //in seconds
    int keepcnt = 3;
    int keepinval = 10; //in seconds
    int vif_idx = WIFI_VIF_INDEX_DEFAULT;
    int send_timeout; // ms
    int send_cnt;
    struct linger ling;
    int close_fd = -1;

    int local_sock_recv = -1;
    int local_port = 1635;
    struct sockaddr_in local_addr_recv;
    struct sockaddr_in local_addr_send;
    int local_recv_sz = 0;
#define LOACL_RECV_BUF_SIZE 50 // in bytes
    uint8_t local_recv_buf[LOACL_RECV_BUF_SIZE] = {0};

    local_sock_recv = socket(AF_INET, SOCK_DGRAM, 0);
    if (local_sock_recv < 0) {
        AT_TRACE("Create local socket recv error!\r\n");
        goto Exit;
    }
    sys_memset(&local_addr_recv, 0, sizeof(local_addr_recv));
    local_addr_recv.sin_family = AF_INET;
    local_addr_recv.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_addr_recv.sin_port = htons(local_port);
    if (bind(local_sock_recv, (struct sockaddr *)&local_addr_recv, sizeof(local_addr_recv)) < 0) {
        AT_TRACE("bind local socket fail. %d!\r\n", errno);
        goto Exit;
    }
    local_sock_send = socket(AF_INET, SOCK_DGRAM, 0);
    if (local_sock_send < 0) {
        AT_TRACE("Create local socket send error!\r\n");
        goto Exit;
    }
    sys_memset(&local_addr_send, 0, sizeof(local_addr_send));
    local_addr_send.sin_family = AF_INET;
    local_addr_send.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_addr_send.sin_port = htons(local_port);
    if (connect(local_sock_send, (struct sockaddr *)&local_addr_send, sizeof(local_addr_send)) < 0) {
        AT_TRACE("connect local socket fail. %d!\r\n", errno);
        goto Exit;
    }

#ifdef CONFIG_ATCMD_SPI
        // init recv data list
        for (i = 0; i < MAX_CLIENT_NUM; i++) {
            if (cip_info.cli[i].fd >= 0) {
                list_init(&cip_info.cli[i].recv_data_list);
                sys_mutex_init(&cip_info.cli[i].list_lock);
            }
        }
#endif

    rx_buf = sys_zalloc(rx_len);
    if(NULL == rx_buf){
        AT_TRACE("Allocate client buffer failed (len = %u).\r\n", rx_len);
        goto Exit;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    cip_task_terminate = 0;
    while (1) {
        if (cip_task_terminate)
            break;

        FD_ZERO(&read_set);
        FD_ZERO(&except_set);
        if (cip_info.local_srv_fd >= 0) {
            if (cip_info.local_srv_stop == 0) {
                FD_SET(cip_info.local_srv_fd, &read_set);
                FD_SET(cip_info.local_srv_fd, &except_set);
                if (cip_info.local_srv_fd > max_fd_num)
                    max_fd_num = cip_info.local_srv_fd;
            } else {
                for (i = 0; i < MAX_CLIENT_NUM; i++) {
                    if ((cip_info.cli[i].fd >= 0) && (cip_info.cli[i].role == CIP_ROLE_SERVER)) {
                        close_fd = cip_info.cli[i].fd;
                        cip_info_cli_free(i);
                        close(close_fd);
                    }
                }
                close_fd = cip_info.local_srv_fd;
                cip_info.local_srv_fd = -1;
                cip_info.local_srv_port = 0;
                close(close_fd);
            }
        }
        for (i = 0; i < MAX_CLIENT_NUM; i++) {
            //if ((cip_info.cli[i].fd >= 0) && (cip_info.cli[i].type == CIP_TYPE_TCP)) {
            if (cip_info.cli[i].fd >= 0) {
                FD_SET(cip_info.cli[i].fd, &read_set);
                FD_SET(cip_info.cli[i].fd, &except_set);
                if (cip_info.cli[i].fd > max_fd_num)
                    max_fd_num = cip_info.cli[i].fd;
            }
        }
        FD_SET(local_sock_recv, &read_set);
        if (local_sock_recv > max_fd_num) {
            max_fd_num = local_sock_recv;
        }
        status = select(max_fd_num + 1, &read_set, NULL, &except_set, &timeout);
        if ((cip_info.local_srv_fd >= 0) && FD_ISSET(cip_info.local_srv_fd, &read_set)) {
            if (cip_info.cli_num >= MAX_CLIENT_NUM) {
                AT_TRACE("client full\r\n");
            } else {
#ifndef CONFIG_ATCMD_SPI
                if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
                        cip_info_valid_tcp_fd_cnt_get() >= 1) {
                    AT_TRACE("Only one TCP client is allowed in Passthrough mode\r\n");
                } else {
#endif
                    /* waiting for an incoming TCP connection */
                    /* accepts a connection form a TCP client, if there is any. otherwise returns SL_EAGAIN */
                    cli_fd = accept(cip_info.local_srv_fd,
                                    (struct sockaddr *)&saddr,
                                    (socklen_t*)&addr_sz);
                    if (cli_fd >= 0) {
                        AT_TRACE("new client %d\r\n", cli_fd);
                        status = cip_info_cli_store(cli_fd, "TCP", CIP_ROLE_SERVER,
                                            saddr.sin_addr.s_addr, saddr.sin_port, cip_info.local_srv_port);
                        if (status < 0) {
                            AT_TRACE("Store client info error %d!\r\n", status);
                            close(cli_fd);
                        } else {
                            setsockopt(cli_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(int));
                            setsockopt(cli_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
                            setsockopt(cli_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepinval, sizeof(int));
                            setsockopt(cli_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));

                            send_timeout = 3000;
                            setsockopt(cli_fd, SOL_SOCKET, SO_SNDTIMEO, (const void *)&send_timeout, sizeof(send_timeout));

                            ling.l_onoff = 1;  // enable
                            ling.l_linger = 3; // in seconds
                            setsockopt(cli_fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
                        }

                        cip_passth_info.passth_fd_idx = status;
                    } else {
                        AT_TRACE("accept error %d!\r\n", errno);
                    }
#ifndef CONFIG_ATCMD_SPI
                }
#endif
            }
        }

        if (FD_ISSET(local_sock_recv, &read_set)) {
            sys_memset(local_recv_buf, 0, LOACL_RECV_BUF_SIZE);
            local_recv_sz = recvfrom(local_sock_recv, local_recv_buf, LOACL_RECV_BUF_SIZE, 0, NULL, NULL);
            if (local_recv_sz <= 0) {
                AT_TRACE("recv data from local fail, %d!\r\n", errno);
            } else {
                if (*((uint16_t *)local_recv_buf) == AT_LOCAL_TCP_SNED_EVENT) {
                    struct at_local_tcp_send *send_data_local = (struct at_local_tcp_send *)local_recv_buf;
                    AT_RSP_START(128);
TCP_RETRY_SEND:
                    send_cnt = send(send_data_local->sock_fd, (void *)(send_data_local->send_data_addr), send_data_local->send_data_len, 0);
                    if (send_cnt <= 0) {
                        AT_TRACE("send data error. %d!\r\n", errno);
                        if (errno == EAGAIN || errno == ENOMEM) {
                            goto TCP_RETRY_SEND;
                        }
                        int idx = cip_info_cli_find(send_data_local->sock_fd);
                        if ((idx != -1) && (cip_info.cli[idx].role == CIP_ROLE_CLIENT)) {
                            cip_info_cli_free(idx);
                            close(send_data_local->sock_fd);
                            AT_TRACE("close tcp client. %d!\r\n", send_data_local->sock_fd);
                        }
                        AT_RSP("SEND FAIL\r\n");
                        AT_RSP_ERR();
                    } else {
                        AT_RSP("SEND OK\r\n");
                        AT_RSP_OK();
                    }
                    sys_mfree((void *)(send_data_local->send_data_addr));
                } else if (*((uint16_t *)local_recv_buf) == AT_LOCAL_UDP_SNED_EVENT) {
                    struct at_local_udp_send *send_data_local = (struct at_local_udp_send *)local_recv_buf;
                    AT_RSP_START(128);
UDP_RETRY_SEND:
                    send_cnt = sendto(send_data_local->sock_fd, (void *)(send_data_local->send_data_addr), send_data_local->send_data_len,
                                        0, (struct sockaddr *)&(send_data_local->to), send_data_local->tolen);
                    if (send_cnt <= 0) {
                        AT_TRACE("send data error. %d!\r\n", errno);
                        if (errno == EAGAIN || errno == ENOMEM) {
                            goto UDP_RETRY_SEND;
                        }
                        int idx = cip_info_cli_find(send_data_local->sock_fd);
                        cip_info_cli_free(idx);
                        close(send_data_local->sock_fd);
                        AT_TRACE("close udp client. %d!\r\n", send_data_local->sock_fd);
                        AT_RSP("SEND FAIL\r\n");
                        AT_RSP_ERR();
                    } else {
                        AT_RSP("SEND OK\r\n");
                        AT_RSP_OK();
                    }
                    sys_mfree((void *)(send_data_local->send_data_addr));
                } else {
                    AT_TRACE("unvalid loacl event.\r\n");
                }
            }
        }
        /*if ((cip_info.local_srv_fd >= 0) && FD_ISSET(cip_info.local_srv_fd, &except_set)) {
            tcp_server_stop();
        }*/
        for (i = 0; i < MAX_CLIENT_NUM; i++) {
            if ((cip_info.cli[i].fd >= 0) && FD_ISSET(cip_info.cli[i].fd, &read_set)) {
                sys_memset(rx_buf, 0, rx_len);
                if (cip_info.cli[i].type == CIP_TYPE_TCP) {
                    recv_sz = recv(cip_info.cli[i].fd, rx_buf, rx_len, 0);
                } else {
                    sys_memset(&saddr, 0, sizeof(saddr));
                    recv_sz = recvfrom(cip_info.cli[i].fd, rx_buf, rx_len,
                                        0, (struct sockaddr *)&saddr, (socklen_t*)&addr_sz);
                }
                //AT_TRACE("RX:%d, %d\r\n", cip_info.cli[i].fd, recv_sz);
                if (recv_sz < 0) { /* Recv error */
                    AT_TRACE("rx error %d\r\n", recv_sz);
                    if (errno == ECONNABORTED) {
                        AT_TRACE("connection aborted, maybe remote close.\r\n");
                    }
                    close_fd = cip_info.cli[i].fd;
                    cip_info_cli_free(i);
                    close(close_fd);
                } else if (recv_sz == 0) {
                    AT_TRACE("remote close %d\r\n", cip_info.cli[i].fd);
                    close(cip_info.cli[i].fd);
#ifndef CONFIG_ATCMD_SPI
                    if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
                            cip_passth_info.passth_fd_idx == i) {
                        cip_passth_info.terminate_send_passth = 1;
                    }
#else
                    if (cip_info.trans_mode == CIP_TRANS_MODE_FILE_TRANSFER &&
                            cip_file_trans_info.fd_idx == i) {
                        cip_file_trans_info.terminate = 1;
                    }
#endif
                    cip_info_cli_free(i);
                } else {
#ifdef CONFIG_ATCMD_SPI
                    // Discard packets during file transfer
                    if (cip_info.trans_mode == CIP_TRANS_MODE_FILE_TRANSFER &&
                        cip_file_trans_info.terminate == 1)
                        break;
#else
                    if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
                            cip_passth_info.passth_fd_idx == i) {
                        AT_RSP_DIRECT(rx_buf, recv_sz);
                    }
#endif
                    if (cip_info.trans_mode == CIP_TRANS_MODE_NORMAL) {
#ifdef CONFIG_ATCMD_SPI
                        at_spi_recv_data_process(i, (uint8_t *)rx_buf, recv_sz);
#else
                        AT_RSP_START(64 + recv_sz);
                        AT_RSP("+IPD,%d,%d: ", cip_info.cli[i].fd, recv_sz);
                        for (j = 0; j < recv_sz; j++)
                            AT_RSP("%c", rx_buf[j]);
                        AT_RSP("\r\n");
                        AT_RSP_OK();
#endif
                    }
                }
            }
            if ((cip_info.cli[i].fd >= 0) && (FD_ISSET(cip_info.cli[i].fd, &except_set) ||
                (wifi_vif_is_softap(vif_idx) && !dhcpd_ipaddr_is_valid(cip_info.cli[i].remote_ip)))) {
                close_fd = cip_info.cli[i].fd;
                AT_TRACE("error %d\r\n", cip_info.cli[i].fd);
                cip_info_cli_free(i);
                close(close_fd);
            }
#ifdef CONFIG_ATCMD_SPI
            sys_enter_critical();
            if (!list_is_empty(&cip_info.cli[i].recv_data_list) && at_spi_hw_is_idle()) {
                spi_handshake_rising_trigger();
                if (spi_nss_status_get() == RESET)
                    printf("nss corner case\r\n");
            }
            sys_exit_critical();
#endif
            if ((cip_info.cli[i].fd >= 0) && (cip_info.cli[i].stop_flag == 1)) {
                close_fd = cip_info.cli[i].fd;
                cip_info_cli_free(i);
                close(close_fd);
                AT_TRACE("close %d.\r\n", close_fd);
            }
        }
    }

    /* Exit */
    for (i = 0; i < MAX_CLIENT_NUM; i++) {
        if (cip_info.cli[i].fd >= 0) {
            close_fd = cip_info.cli[i].fd;
            cip_info_cli_free(i);
            close(close_fd);
        }
    }
    /* close the listening socket */
    if (cip_info.local_srv_fd >= 0)
        close(cip_info.local_srv_fd);
    cip_info.local_srv_fd = -1;
    cip_info.local_srv_port = 0;

    sys_mfree(rx_buf);

Exit:
    if (local_sock_send >= 0) {
        shutdown(local_sock_send, SHUT_RD);
        close(local_sock_send);
    }
    if (local_sock_recv >= 0) {
        shutdown(local_sock_recv, SHUT_RD);
        close(local_sock_recv);
    }
    sys_task_delete(NULL);
}

/*!
    \brief      the AT command ping
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_ping(int argc, char **argv)
{
    struct ping_info_t *ping_info = NULL;

    AT_RSP_START(128);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            char *domain = at_string_parse(argv[1]);
            struct addrinfo hints, *res;
            void *ptr;
#ifdef CONFIG_IPV6_SUPPORT
            char ip_addr[64];
#else
            char ip_addr[32];
#endif /* CONFIG_IPV6_SUPPORT */
            if (domain == NULL) {
                goto Error;
            }

            memset(&hints, 0, sizeof(hints));
            if (getaddrinfo(domain, NULL, &hints, &res) != 0) {
                goto Error;
            }

            ping_info = sys_zalloc(sizeof(struct ping_info_t));
            if (ping_info == NULL) {
                freeaddrinfo(res);
                goto Error;
            }
#ifdef CONFIG_IPV6_SUPPORT
            if (res->ai_family == AF_INET6) {
                ptr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
                ping_info->ip_type = IPADDR_TYPE_V6;
            } else
#endif /* CONFIG_IPV6_SUPPORT */
            {
                ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            }
            inet_ntop(res->ai_family, ptr, ip_addr, sizeof(ip_addr));
            freeaddrinfo(res);
            memcpy(ping_info->ping_ip, ip_addr, sizeof(ping_info->ping_ip));
            ping_info->ping_cnt = 5;
            ping_info->ping_size = 120;
            ping_info->ping_interval = 1000;
            if (ping(ping_info) != ERR_OK)
                goto Error;
            AT_RSP("%s", ping_info->ping_res);
        }
    } else {
        goto Error;
    }

    if (ping_info)
        sys_mfree(ping_info);

    AT_RSP_OK();
    return;

Error:
    if (ping_info)
        sys_mfree(ping_info);
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+PING=<ip or domain name>\r\n");
    AT_RSP_OK();
    return;
}

/*!
    \brief      the AT command start a tcp or udp client
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_start(int argc, char **argv)
{
    char *type, *srv_ip;
    uint16_t srv_port;
    uint8_t bkeep_alive = 0;
    char *endptr = NULL;
    int ret = -1, fd = -1;
    uint16_t local_port = 0;

    AT_RSP_START(128);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            goto Error;
        }
    } else if ((argc == 4) || (argc == 5)) {
        type = at_string_parse(argv[1]);
        srv_ip = at_string_parse(argv[2]);
        if ((type == NULL) || (srv_ip == NULL)) {
            goto Error;
        }
        srv_port = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        if (argc == 5) {
            if (strncmp(type, "TCP", 3) == 0) {
                bkeep_alive = (uint32_t)strtoul((const char *)argv[4], &endptr, 10);
                if (*endptr != '\0') {
                    goto Error;
                }
            } else if (strncmp(type, "UDP", 3) == 0) {
                local_port = (uint32_t)strtoul((const char *)argv[4], &endptr, 10);
                if (*endptr != '\0') {
                    goto Error;
                }
            }
        }
        if (cip_info.cli_num >= MAX_CLIENT_NUM) {
            AT_TRACE("client full\r\n");
            goto Error;
        }

        if (strncmp(type, "TCP", 3) == 0) {
            if ((ret = tcp_client_start(srv_ip, srv_port, bkeep_alive, &fd) < 0))
                goto Error;
        } else if (strncmp(type, "UDP", 3) == 0) {
            if ((ret = udp_client_start(srv_ip, srv_port, local_port, &fd) < 0))
                goto Error;
        } else {
            goto Error;
        }
        if (cip_task_started == 0) {
            if (sys_task_create_dynamic((const uint8_t *)"Cip Rcv",
                            CIP_RECV_STACK_SIZE, CIP_RECV_PRIO,
                            (task_func_t)cip_recv_task, NULL) == NULL)
                goto Error;
            cip_task_started = 1;
        }
    } else {
        goto Error;
    }
    AT_RSP("%d,", fd);
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+CIPSTART=<type:TCP or UDP>,<remote ip>,<remote port>,[udp local port],[tcp keep alive:0-1]\r\n");
    AT_RSP_OK();
    return;
}


/*!
    \brief      the AT command send a tcp or udp packet
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_send(int argc, char **argv)
{
    int fd, idx;
    uint32_t tx_len, file_len, segment_len;
    char *srv_ip;
    uint16_t srv_port;
    char *endptr = NULL;

    AT_RSP_START(128);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            goto Error;
        }
    } else if (argc == 3) {
        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            AT_TRACE("fd error\r\n");
            goto Error;
        }
        tx_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (tx_len > 2048)) {
            goto Error;
        }
        //AT_TRACE("FD: %d, len %d\r\n", fd, tx_len);
        if (cip_info.cli[idx].type == CIP_TYPE_TCP) {
            if (at_tcp_send(fd, tx_len) <= 0) {
                goto Error;
            }
        } else {
            AT_TRACE("ip error\r\n");
            goto Error;
        }
        AT_RSP_FREE();
        return;
    } else if (argc == 5) {
        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            goto Error;
        }
        tx_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (tx_len > 2048)) {
            goto Error;
        }
        srv_ip = at_string_parse(argv[3]);
        if (srv_ip == NULL) {
            goto Error;
        }
        srv_port = (uint32_t)strtoul((const char *)argv[4], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        AT_TRACE("FD: %d, len %d, ip %s, port %d\r\n", fd, tx_len, srv_ip, srv_port);
        if (cip_info.cli[idx].type == CIP_TYPE_TCP) {
            if (at_tcp_send(fd, tx_len) <= 0) {
                goto Error;
            }
        } else {
            if (at_udp_send(fd, tx_len, srv_ip, srv_port) <= 0) {
                goto Error;
            }
        }
        AT_RSP_FREE();
        return;
#ifndef CONFIG_ATCMD_SPI
    } else if (argc == 1) {
        if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH) {
            if (cip_info_valid_tcp_fd_cnt_get() > 1) {
                AT_TRACE("TCP Passthrough mode support only 1 TCP connection\r\n");
                goto Error;
            }

            idx = cip_passth_info.passth_fd_idx;
            if (idx == -1 || cip_info.cli[idx].fd < 0 || cip_info_valid_fd_cnt_get() == 0) {
                AT_TRACE("Invalid Passthrough fd\r\n");
                goto Error;
            }

            // AT_TRACE("PassThrough SEND mode enter, fd=%d\r\n", cip_info.cli[idx].fd);
            AT_RSP("OK\r\n");
            AT_RSP(">\r\n");
            AT_RSP_DIRECT(rsp_buf, rsp_buf_idx);

            at_hw_passth_send(cip_info.cli[idx].fd, cip_info.cli[idx].type);

            return;
        } else {
            goto Error;
        }
#endif
#ifdef CONFIG_ATCMD_SPI
    } else if (argc == 4) {
        if (cip_info.trans_mode != CIP_TRANS_MODE_FILE_TRANSFER)
            goto Error;

        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            goto Error;
        }
        file_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (file_len > FILE_MAX_LEN)) {
            goto Error;
        }

        segment_len = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if ((*endptr != '\0') || (segment_len > FILE_MAX_SEGMENT_LEN)) {
            goto Error;
        }
        AT_TRACE("FD: %d, flen %d, slen %d\r\n", fd, file_len, segment_len);
        if (cip_file_transfer_info_init(idx, file_len, segment_len))
            goto Error;
        AT_RSP_OK();

        at_send_file(idx, file_len, segment_len);

        return;
    } else if (argc == 6) {
        if (cip_info.trans_mode != CIP_TRANS_MODE_FILE_TRANSFER)
            goto Error;

        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            goto Error;
        }
        file_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (file_len > FILE_MAX_LEN)) {
            goto Error;
        }
        segment_len = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if ((*endptr != '\0') || (segment_len > FILE_MAX_SEGMENT_LEN)) {
            goto Error;
        }
        srv_ip = at_string_parse(argv[4]);
        if (srv_ip == NULL) {
            goto Error;
        }
        srv_port = (uint32_t)strtoul((const char *)argv[5], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        AT_TRACE("FD: %d, flen %d, slen %d, ip %s, port %d\r\n", fd, file_len, segment_len, srv_ip, srv_port);

        if (cip_file_transfer_info_init(idx, file_len, segment_len))
            goto Error;

        AT_RSP_OK();
        idx = 0;
        at_send_file(idx, file_len, segment_len);

        return;
#endif
    } else {
        goto Error;
    }
    AT_RSP("SEND OK\r\n");
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("Usage:\r\n");
    AT_RSP("Normal Mode Usage:\r\n");
    AT_RSP("    +CIPSEND=<fd:0-4>,<len>,[<remote ip>,<remote port>]\r\n");
#ifndef CONFIG_ATCMD_SPI
    AT_RSP("PassThrough Mode Usage:\r\n");
    AT_RSP("    +CIPSEND\r\n");
#else
    AT_RSP("FileTransfer Mode Usage:\r\n");
    AT_RSP("    +CIPSEND=<fd:0-4>,<file_len>,<segment_len>,[<remote ip>,<remote port>]\r\n");
#endif
    AT_RSP_OK();
    return;
}

#ifdef CONFIG_ATCMD_SPI

/*!
    \brief      the AT command send a tcp or udp packet
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_send_file(int argc, char **argv)
{
    int fd, idx;
    uint32_t tx_len, file_len, segment_len;
    char *srv_ip;
    uint16_t srv_port;
    char *endptr = NULL;

    AT_RSP_START(128);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            goto Error;
        }
    } else if (argc == 4) {
        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            goto Error;
        }
        file_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (file_len > FILE_MAX_LEN)) {
            goto Error;
        }

        segment_len = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if ((*endptr != '\0') || (segment_len > FILE_MAX_SEGMENT_LEN)) {
            goto Error;
        }

        AT_TRACE("FD: %d, flen %d, slen %d\r\n", fd, file_len, segment_len);
        if (cip_file_transfer_info_init(idx, file_len, segment_len))
            goto Error;

        AT_RSP_OK();

        at_send_file(idx, file_len, segment_len);

        return;
    }else if (argc == 6) {
        if (cip_info.trans_mode != CIP_TRANS_MODE_FILE_TRANSFER)
            goto Error;

        fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
        idx = cip_info_cli_find(fd);
        if ((*endptr != '\0') || idx < 0) {
            goto Error;
        }
        file_len = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
        if ((*endptr != '\0') || (file_len > FILE_MAX_LEN)) {
            goto Error;
        }
        segment_len = (uint32_t)strtoul((const char *)argv[3], &endptr, 10);
        if ((*endptr != '\0') || (segment_len > FILE_MAX_SEGMENT_LEN)) {
            goto Error;
        }
        srv_ip = at_string_parse(argv[4]);
        if (srv_ip == NULL) {
            goto Error;
        }
        srv_port = (uint32_t)strtoul((const char *)argv[5], &endptr, 10);
        if (*endptr != '\0') {
            goto Error;
        }
        AT_TRACE("FD: %d, flen %d, slen %d, ip %s, port %d\r\n", fd, file_len, segment_len, srv_ip, srv_port);

        if (cip_file_transfer_info_init(idx, file_len, segment_len))
            goto Error;

        AT_RSP_OK();

        idx = 0;
        at_send_file(idx, file_len, segment_len);

        return;
    } else {
        goto Error;
    }
    AT_RSP("SEND OK\r\n");
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("Usage:\r\n");

    AT_RSP("FileTransfer Mode Usage:\r\n");
    AT_RSP("    +CIPSEND=<fd:0-4>,<file_len>,<segment_len>,[<remote ip>,<remote port>]\r\n");

    AT_RSP_OK();
    return;
}
#endif

#ifdef CONFIG_ATCMD_SPI
void at_cip_recvdata(int argc, char **argv)
{
    int recv_len;
    int fd = -1, idx = -1;
    struct recv_data_node *p_item;
    uint8_t *data_remain = NULL;

    AT_RSP_START(AT_SPI_MAX_DATA_LEN + 30);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            for (idx = 0; idx < MAX_CLIENT_NUM; idx++) {
                if (cip_info.cli[idx].fd >= 0) {
                    if (!list_is_empty(&cip_info.cli[idx].recv_data_list)) {
                        fd = cip_info.cli[idx].fd; // when found the first fd's recv data is not none, break
                        break;
                    }
                }
            }

            if (idx < MAX_CLIENT_NUM && fd >= 0) {
                recv_len = atoi(argv[1]);
                if (recv_len < 0 || recv_len > AT_SPI_MAX_DATA_LEN) {
                    app_print("recv_len:%d error\r\n", recv_len);
                    goto Error;
                }

                sys_mutex_get(&cip_info.cli[idx].list_lock);
                p_item = (struct recv_data_node *)list_pop_front(&cip_info.cli[idx].recv_data_list);

                if ((p_item != NULL) && p_item->data && (p_item->data_len > 0)) {
                    if (p_item->data_len <= recv_len) { // data length is smaller than request data length
                        AT_RSP("+CIPRECVDATA:%d,%d,", fd, p_item->data_len);

                        // copy data to AT_RSP
                        sys_memcpy((rsp_buf + rsp_buf_idx), p_item->data, p_item->data_len);
                        rsp_buf_idx += p_item->data_len;

                        // AT_TRACE("CIPRECVDATA,len:%d\r\n", p_item->data_len);
                        sys_mfree(p_item->data);
                        sys_mfree(p_item);
                    } else {                            // data length is larger than request data length
                        AT_RSP("+CIPRECVDATA:%d,%d,", fd, recv_len);

                        // copy data to AT_RSP
                        sys_memcpy(rsp_buf + rsp_buf_idx, p_item->data, p_item->data_len);
                        rsp_buf_idx += p_item->data_len;
                        // AT_TRACE("CIPRECVDATA, len:%d\r\n", recv_len);

                        // copy remain data to a new buff, and then add it to list header again.
                        data_remain = sys_malloc(p_item->data_len - recv_len);
                        if (data_remain == NULL) {
                            AT_TRACE("data_remain malloc failed, len:%d\r\n", (p_item->data_len - recv_len));
                            sys_mfree(p_item->data); // when malloc fail, drop this data
                            sys_mfree(p_item);
                            goto Error;
                        }
                        sys_memcpy(data_remain, (p_item->data + recv_len), (p_item->data_len - recv_len));
                        sys_mfree(p_item->data);
                        p_item->data = data_remain;
                        p_item->data_len = (p_item->data_len - recv_len);
                        list_push_front(&cip_info.cli[idx].recv_data_list, &p_item->list_hdr);
                    }
                }
                sys_mutex_put(&cip_info.cli[idx].list_lock);
            } else {
                AT_RSP("+CIPRECVDATA:-1,0");
            }
        }
    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("Usage:\r\n");
    AT_RSP("    +CIPRECVDATA=<fd:0-4>\r\n");
    AT_RSP_OK();
    return;
}
#endif
/*!
    \brief      the AT command start or stop a tcp server
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_server(int argc, char **argv)
{
    uint8_t enable;
    uint16_t port = 0;
    char *endptr = NULL;

    AT_RSP_START(128);
    if ((argc == 2) || (argc == 3)) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            enable = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
            if ((*endptr != '\0') || (enable > 1)) {
                goto Error;
            }
            if (argc == 3) {
                port = (uint32_t)strtoul((const char *)argv[2], &endptr, 10);
                if (*endptr != '\0') {
                    goto Error;
                }
            }
            if (enable) {
#ifndef CONFIG_ATCMD_SPI
                if (cip_info.trans_mode == CIP_TRANS_MODE_PASSTHROUGH &&
                    (cip_info_valid_fd_cnt_get() > 0 || cip_info.local_srv_fd >= 0))
                    goto Error;
#endif
                if (cip_info.local_srv_fd >= 0) {
                    AT_TRACE("Already run\r\n");
                    goto Error;
                }
                if (tcp_server_start(port) < 0) {
                    goto Error;
                }
                if (cip_task_started == 0) {
                    if (sys_task_create_dynamic((const uint8_t *)"Cip Rcv",
                                    CIP_RECV_STACK_SIZE, CIP_RECV_PRIO,
                                    (task_func_t)cip_recv_task, NULL) == NULL)
                        goto Error;
                    cip_task_started = 1;
                }
            } else {
                tcp_server_stop();
            }
        }
    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+CIPSERVER=<mode:0-1>,[port]\r\n");
    AT_RSP_OK();
    return;
}

/*!
    \brief      the AT command free tcpip information, close client
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_close(int argc, char **argv)
{
    int fd, i;
    char *endptr = NULL;
    int found = -1;
    int active_sock_num = 0;

    AT_RSP_START(128);
    if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            fd = (uint32_t)strtoul((const char *)argv[1], &endptr, 10);
            if ((*endptr != '\0') || (fd < 0)) {
                goto Error;
            }
            if (fd == cip_info.local_srv_fd) {
                AT_TRACE("server fd\r\n");
                goto Error;
            }
            for (i = 0; i < MAX_CLIENT_NUM; i++) {
                if (fd == cip_info.cli[i].fd) {
                    found = i;
                }
                if (cip_info.cli[i].fd > -1) {
                    active_sock_num++;
                }
            }
            if (found == -1) {
                AT_TRACE("can not find fd.\r\n");
                goto Error;
            }
            if (cip_info.local_srv_fd != -1) {
                active_sock_num++;
            }
            if (active_sock_num > 1) {
                cip_info.cli[found].stop_flag = 1;
            } else {
                cip_task_terminate = 1;
                while (sys_task_exist((const uint8_t *)"Cip Rcv")) {
                    sys_ms_sleep(1);
                }
                cip_task_started = 0;
            }
        }
    } else {
        goto Error;
    }
    AT_RSP("close %d\r\n", fd);
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+CIPCLOSE=<fd>\r\n");
    AT_RSP_OK();
    return;
}

/*!
    \brief      the AT command show tcpip information status
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_status(int argc, char **argv)
{
    int i = 0;
    char type[4];
    int vif_idx = WIFI_VIF_INDEX_DEFAULT;

    AT_RSP_START(512);
    if (argc == 1) {
        if (wifi_vif_is_sta_connected(vif_idx)) {
            if (cip_info.cli_num > 0) {
                AT_RSP("STATUS: 3\r\n");
            } else {
                AT_RSP("STATUS: 2\r\n");
            }
        } else if (wifi_vif_is_sta_handshaked(vif_idx)) {
            AT_RSP("STATUS: 4\r\n");
        } else {
            AT_RSP("STATUS: 5\r\n");
        }

        for (i = 0; i < MAX_CLIENT_NUM; i++) {
            if (cip_info.cli[i].fd >= 0) {
                if (cip_info.cli[i].type == CIP_TYPE_TCP)
                    strcpy(type, "TCP");
                else
                    strcpy(type, "UDP");
                AT_RSP("+CIPSTATUS:%d,%s,"IP_FMT",%d,%d,%d\r\n",
                        cip_info.cli[i].fd, type, IP_ARG(cip_info.cli[i].remote_ip),
                        cip_info.cli[i].remote_port, cip_info.cli[i].local_port, cip_info.cli[i].role);
            }
        }
    } else {
        goto Error;
    }
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
}


/*!
    \brief      the AT command set transfer interval
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/

void at_trans_interval(int argc, char **argv)
{
    AT_RSP_START(32);

    if (argc == 1) {
        AT_RSP("+TRANSINTVAL:%d\r\n", cip_info.trans_intvl);
    } else if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            cip_info.trans_intvl = atoi(argv[1]);
        }
    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+TRANSINTVAL=<interval>\r\n");
    AT_RSP_OK();
    return;
}

/*!
    \brief      the AT command set transfer mode
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/

void at_cip_mode(int argc, char **argv)
{
    int mode;
    int fd, type, idx;

    AT_RSP_START(32);
    if (argc == 1) {
        AT_RSP("+CIPMODE:%d\r\n", cip_info.trans_mode);
    } else if (argc == 2) {
        if (argv[1][0] == AT_QUESTION) {
            goto Usage;
        } else {
            mode = atoi(argv[1]);
            if (mode == 0) {
                cip_info.trans_mode = CIP_TRANS_MODE_NORMAL;
#ifndef CONFIG_ATCMD_SPI
            } else if (mode == CIP_TRANS_MODE_PASSTHROUGH) {
                if (cip_info_valid_tcp_fd_cnt_get() >= 2) {
                    AT_TRACE("TCP Passthrough mode support only 1 TCP connection\r\n");
                    goto Error;
                }

                if (cip_passth_info.passth_fd_idx == -1) {
                    AT_TRACE("Invalid Passthrough fd\r\n");
                    goto Error;
                }
                cip_info.trans_mode = CIP_TRANS_MODE_PASSTHROUGH;
#else
            } else if (mode == 2) {
                if (cip_info_valid_tcp_fd_cnt_get() >= 2) {
                    AT_TRACE("File transfer mode support only 1 TCP connection\r\n");
                    goto Error;
                }
                cip_info.trans_mode = CIP_TRANS_MODE_FILE_TRANSFER;
#endif
            } else {
                AT_TRACE("Unknown transfer mode:%d\r\n", mode);
                goto Error;
            }
        }
    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
#ifndef CONFIG_ATCMD_SPI
    AT_RSP("+CIPMODE=<mode:0-1>\r\n");
#else
    AT_RSP("+CIPMODE=<mode:0-1>\r\n");
#endif
    AT_RSP_OK();
    return;
}

int at_parse_ip4(char *str, uint32_t *ip)
{
    char *token;
    uint32_t a, i, j;

    token = strchr(str, '/');
    *ip = 0;

    for (i = 0; i < 4; i++) {
        if (i < 3) {
            token = strchr(str, '.');
            if (!token)
                return -1;
            *token++ = '\0';
        }
        for (j = 0; j < strlen(str); j++) {
            if (str[j] < '0' || str[j] > '9')
            return -1;
        }

        a = atoi(str);
        if (a > 255)
            return -1;
        str = token;
        *ip += (a << (i * 8));
    }

    return 0;
}

/*!
    \brief      the AT command set station ip
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_sta_ip(int argc, char **argv)
{
    struct wifi_ip_addr_cfg ip_cfg;

    AT_RSP_START(256);
    if (argc == 1) {
        if (!wifi_get_vif_ip(WIFI_VIF_INDEX_DEFAULT, &ip_cfg))
        {
            AT_RSP("+CIPSTA: "IP_FMT"\r\n", IP_ARG(ip_cfg.ipv4.addr));
            AT_RSP("+CIPSTA: "IP_FMT"\r\n", IP_ARG(ip_cfg.ipv4.mask));
            AT_RSP("+CIPSTA: "IP_FMT"\r\n", IP_ARG(ip_cfg.ipv4.gw));
#ifdef CONFIG_IPV6_SUPPORT
            {
                char ip6_local[IPV6_ADDR_STRING_LENGTH_MAX] = {0};
                char ip6_unique[IPV6_ADDR_STRING_LENGTH_MAX] = {0};
                if (!wifi_get_vif_ip6(WIFI_VIF_INDEX_DEFAULT, ip6_local, ip6_unique)) {
                    AT_RSP("+CIPSTA: [%s]\r\n", ip6_local);
                    AT_RSP("+CIPSTA: [%s]\r\n", ip6_unique);
                }
            }
#endif
        } else {
            goto Usage;
        }

    } else if (argc == 2) {
        if (argv[1][0] == AT_QUESTION)
            goto Usage;
        else
            goto Error;

    } else if (argc == 4) {
        ip_cfg.mode = IP_ADDR_STATIC_IPV4;
        net_if_use_static_ip(true);
        if (at_parse_ip4(at_string_parse(argv[1]), &ip_cfg.ipv4.addr))
            goto Usage;
        if (at_parse_ip4(at_string_parse(argv[2]), &ip_cfg.ipv4.mask))
            goto Usage;
        if (at_parse_ip4(at_string_parse(argv[3]), &ip_cfg.ipv4.gw))
            goto Usage;
        AT_TRACE("+CIPSTA: set "IP_FMT", "IP_FMT", "IP_FMT"\r\n",
                    IP_ARG(ip_cfg.ipv4.addr), IP_ARG(ip_cfg.ipv4.mask), IP_ARG(ip_cfg.ipv4.gw));

        wifi_set_vif_ip(WIFI_VIF_INDEX_DEFAULT, &ip_cfg);

    } else {
        goto Error;
    }

    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
Usage:
    AT_RSP("+CIPSTA=<ip>,<netmask>,<gw>\r\n");
    AT_RSP_OK();
    return;
}

/*!
    \brief      the AT command show ap and station ip
    \param[in]  argc: number of parameters
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
void at_cip_ip_addr_get(int argc, char **argv)
{
    struct wifi_vif_tag *wvif = (struct wifi_vif_tag *)vif_idx_to_wvif(WIFI_VIF_INDEX_DEFAULT);
    struct wifi_ip_addr_cfg ip_cfg;

    AT_RSP_START(256);
    if (argc == 1) {
        if (wifi_get_vif_ip(WIFI_VIF_INDEX_DEFAULT, &ip_cfg)) {
            goto Error;
        }
        if (wvif->wvif_type == WVIF_AP) {
            AT_RSP("+CIFSR:APIP,"IP_FMT"\r\n", IP_ARG(ip_cfg.ipv4.addr));
            AT_RSP("+CIFSR:APMAC,"MAC_FMT"\r\n", MAC_ARG(wvif->mac_addr.array));
        } else if(wvif->wvif_type == WVIF_STA) {
            AT_RSP("+CIFSR:STAIP,"IP_FMT"\r\n", IP_ARG(ip_cfg.ipv4.addr));
            AT_RSP("+CIFSR:STAMAC,"MAC_FMT"\r\n", MAC_ARG(wvif->mac_addr.array));
        }
    } else {
        goto Error;
    }
    AT_RSP_OK();
    return;

Error:
    AT_RSP_ERR();
    return;
}
