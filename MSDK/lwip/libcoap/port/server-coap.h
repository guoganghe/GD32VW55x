/*
 * server-coap.h -- LwIP example
 *
 * Copyright (C) 2013-2016 Christian Amsüss <chrysn@fsfe.org>
 * Copyright (c) 2024, GigaDevice Semiconductor Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#include "coap_config.h"
#include <coap3/coap.h>

/* Start up the CoAP Server */
void server_coap_init(void);

/* Close down CoAP activity */

void server_coap_finished(void);

/* call this when you think that resources could be dirty */
int server_coap_poll(void);
