/**
 * \file ipfix.c
 * \date 2019
 * \author Jiri Havranek <havranek@cesnet.cz>
 */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.

 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
*/

//The following code was used https://dior.ics.muni.cz/~velan/flowmon-export-ipfix/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <endian.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "ipfix.h"
#include "plugin.h"
#include "cache.h"


void ipfix_shutdown(struct ipfix_s *ipfix)
{
   /* Close the connection */
   if (ipfix->fd != -1) {
      ipfix_flush(ipfix);

      close(ipfix->fd);
      freeaddrinfo(ipfix->addrinfo);
      ipfix->fd = -1;
   }
   if (ipfix->templateArray != NULL) {
      free(ipfix->templateArray);
      ipfix->templateArray = NULL;
   }

   template_t *tmp = ipfix->templates;
   while (tmp != NULL) {
      ipfix->templates = ipfix->templates->next;
      free(tmp);
      tmp = ipfix->templates;
   }
   tmp = NULL;
}

int ipfix_export_flow(struct ipfix_s *ipfix, struct flowrec_s *flow)
{
   template_t *tmplt;
   uint8_t *buffer;
   size_t bufferSize;
   struct flowext_s *extRec = flow->ext;

   if (extRec == NULL && ipfix->export_basic) { 
      if ((flow[0].ip_version) == (4)) {
         tmplt = ipfix->templateArray[0];
         while (tmplt->bufferSize + 78 > TEMPLATE_BUFFER_SIZE) {
            ipfix_send_templates(ipfix);
            ipfix_send_data(ipfix);
         }
         buffer = tmplt->buffer + tmplt->bufferSize;
         bufferSize = tmplt->bufferSize + 78;
         *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
         buffer += 2;
         *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
         buffer += 8;
         *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
         buffer += 8;
         *(uint64_t *) buffer = ntohll(flow->id);
         buffer += 8;
         *(uint64_t *) buffer = ntohll(flow->parent);
         buffer += 8;
         *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
         buffer += 4;
         *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
         buffer += 4;
      } else {
         tmplt = ipfix->templateArray[1];
         while (tmplt->bufferSize + 102 > TEMPLATE_BUFFER_SIZE) {
            ipfix_send_templates(ipfix);
            ipfix_send_data(ipfix);
         }
         buffer = tmplt->buffer + tmplt->bufferSize;
         bufferSize = tmplt->bufferSize + 102;
         *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
         buffer += 2;
         *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
         buffer += 8;
         *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
         buffer += 8;
         *(uint64_t *) buffer = ntohll(flow->id);
         buffer += 8;
         *(uint64_t *) buffer = ntohll(flow->parent);
         buffer += 8;
         memcpy(buffer, flow[0].src_addr.v6.addr, 16);
         buffer += 16;
         memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
         buffer += 16;
      }
      *(uint8_t *) buffer = (flow[0].ip_version);
      buffer += 1;
      *(uint8_t *) buffer = (flow[0].ttl);
      buffer += 1;
      *(uint64_t *) buffer = ntohll(flow[0].bytes);
      buffer += 8;
      *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
      buffer += 8;
      *(uint8_t *) buffer = (flow[0].protocol);
      buffer += 1;
      *(uint16_t *) buffer = ntohs(flow[0].src_port);
      buffer += 2;
      *(uint16_t *) buffer = ntohs(flow[0].dst_port);
      buffer += 2;
      *(uint8_t *) buffer = (flow[0].tcpflags);
      buffer += 1;
      buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
      buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
      buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
      buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
      buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
      buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
      buffer += 6;
      buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
      buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
      buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
      buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
      buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
      buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
      buffer += 6;
      tmplt->bufferSize = bufferSize;
      tmplt->recordCount++;
   } else {
      while (extRec != NULL) { 
         while (extRec->id == flow_ext_http) {
            size_t str_len;
            struct http_extension_s *ext = (struct http_extension_s *) extRec->data;

            (void) str_len;
            (void) ext;

            if ((flow[0].ip_version) == (4)) {
               tmplt = ipfix->templateArray[2];
               while (tmplt->bufferSize + 86 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 86;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
               buffer += 4;
               *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
               buffer += 4;
            } else {
               tmplt = ipfix->templateArray[3];
               while (tmplt->bufferSize + 110 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 110;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               memcpy(buffer, flow[0].src_addr.v6.addr, 16);
               buffer += 16;
               memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
               buffer += 16;
            }
            *(uint8_t *) buffer = (flow[0].ip_version);
            buffer += 1;
            *(uint8_t *) buffer = (flow[0].ttl);
            buffer += 1;
            *(uint64_t *) buffer = ntohll(flow[0].bytes);
            buffer += 8;
            *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
            buffer += 8;
            *(uint8_t *) buffer = (flow[0].protocol);
            buffer += 1;
            *(uint16_t *) buffer = ntohs(flow[0].src_port);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(flow[0].dst_port);
            buffer += 2;
            *(uint8_t *) buffer = (flow[0].tcpflags);
            buffer += 1;
            buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
            buffer += 6;
            buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
            buffer += 6;
            if ((ext[0].type) == (1)) {
               str_len = strlen((const char *) ext[0].data.req.agent);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.req.agent, str_len);
               bufferSize += str_len;
               buffer += str_len;
               str_len = strlen((const char *) ext[0].data.req.method);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.req.method, str_len);
               bufferSize += str_len;
               buffer += str_len;
               str_len = strlen((const char *) ext[0].data.req.host);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.req.host, str_len);
               bufferSize += str_len;
               buffer += str_len;
               str_len = strlen((const char *) ext[0].data.req.referer);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.req.referer, str_len);
               bufferSize += str_len;
               buffer += str_len;
               str_len = strlen((const char *) ext[0].data.req.uri);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.req.uri, str_len);
               bufferSize += str_len;
               buffer += str_len;
               *buffer = 0;
               buffer++;
               *(uint16_t *) buffer = ntohs(0);
               buffer += 2;
               tmplt->bufferSize = bufferSize;
               tmplt->recordCount++;
            } else {
               *buffer = 0;
               buffer++;
               *buffer = 0;
               buffer++;
               *buffer = 0;
               buffer++;
               *buffer = 0;
               buffer++;
               *buffer = 0;
               buffer++;
               str_len = strlen((const char *) ext[0].data.resp.content_type);
               if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                     ipfix_send_templates(ipfix);
                     ipfix_send_data(ipfix);
                     bufferSize = tmplt->bufferSize;
                  }
                  continue;
               }
               *buffer = str_len;
               buffer += 1;
               memcpy(buffer, ext[0].data.resp.content_type, str_len);
               bufferSize += str_len;
               buffer += str_len;
               *(uint16_t *) buffer = ntohs(ext[0].data.resp.code);
               buffer += 2;
               tmplt->bufferSize = bufferSize;
               tmplt->recordCount++;
            }
            break;
         }
         while (extRec->id == flow_ext_smtp) {
            size_t str_len;
            struct smtp_extension_s *ext = (struct smtp_extension_s *) extRec->data;

            (void) str_len;
            (void) ext;

            if ((flow[0].ip_version) == (4)) {
               tmplt = ipfix->templateArray[4];
               while (tmplt->bufferSize + 113 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 113;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
               buffer += 4;
               *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
               buffer += 4;
            } else {
               tmplt = ipfix->templateArray[5];
               while (tmplt->bufferSize + 137 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 137;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               memcpy(buffer, flow[0].src_addr.v6.addr, 16);
               buffer += 16;
               memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
               buffer += 16;
            }
            *(uint8_t *) buffer = (flow[0].ip_version);
            buffer += 1;
            *(uint8_t *) buffer = (flow[0].ttl);
            buffer += 1;
            *(uint64_t *) buffer = ntohll(flow[0].bytes);
            buffer += 8;
            *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
            buffer += 8;
            *(uint8_t *) buffer = (flow[0].protocol);
            buffer += 1;
            *(uint16_t *) buffer = ntohs(flow[0].src_port);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(flow[0].dst_port);
            buffer += 2;
            *(uint8_t *) buffer = (flow[0].tcpflags);
            buffer += 1;
            buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
            buffer += 6;
            buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
            buffer += 6;
            *(uint32_t *) buffer = ntohl(ext[0].command_flags);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].mail_cmd_cnt);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].mail_rcpt_cnt);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].mail_code_flags);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].code_2xx_cnt);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].code_3xx_cnt);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].code_4xx_cnt);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].code_5xx_cnt);
            buffer += 4;
            str_len = strlen((const char *) ext[0].domain);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].domain, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].first_sender);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].first_sender, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].first_recipient);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].first_recipient, str_len);
            bufferSize += str_len;
            buffer += str_len;
            tmplt->bufferSize = bufferSize;
            tmplt->recordCount++;
            break;
         }
         while (extRec->id == flow_ext_https) {
            size_t str_len;
            struct https_extension_s *ext = (struct https_extension_s *) extRec->data;

            (void) str_len;
            (void) ext;

            if ((flow[0].ip_version) == (4)) {
               tmplt = ipfix->templateArray[6];
               while (tmplt->bufferSize + 79 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 79;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
               buffer += 4;
               *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
               buffer += 4;
            } else {
               tmplt = ipfix->templateArray[7];
               while (tmplt->bufferSize + 103 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 103;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               memcpy(buffer, flow[0].src_addr.v6.addr, 16);
               buffer += 16;
               memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
               buffer += 16;
            }
            *(uint8_t *) buffer = (flow[0].ip_version);
            buffer += 1;
            *(uint8_t *) buffer = (flow[0].ttl);
            buffer += 1;
            *(uint64_t *) buffer = ntohll(flow[0].bytes);
            buffer += 8;
            *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
            buffer += 8;
            *(uint8_t *) buffer = (flow[0].protocol);
            buffer += 1;
            *(uint16_t *) buffer = ntohs(flow[0].src_port);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(flow[0].dst_port);
            buffer += 2;
            *(uint8_t *) buffer = (flow[0].tcpflags);
            buffer += 1;
            buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
            buffer += 6;
            buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
            buffer += 6;
            str_len = strlen((const char *) ext[0].sni);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].sni, str_len);
            bufferSize += str_len;
            buffer += str_len;
            tmplt->bufferSize = bufferSize;
            tmplt->recordCount++;
            break;
         }
         while (extRec->id == flow_ext_ntp) {
            size_t str_len;
            struct ntp_extension_s *ext = (struct ntp_extension_s *) extRec->data;

            (void) str_len;
            (void) ext;

            if ((flow[0].ip_version) == (4)) {
               tmplt = ipfix->templateArray[8];
               while (tmplt->bufferSize + 128 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 128;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
               buffer += 4;
               *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
               buffer += 4;
            } else {
               tmplt = ipfix->templateArray[9];
               while (tmplt->bufferSize + 152 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 152;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               memcpy(buffer, flow[0].src_addr.v6.addr, 16);
               buffer += 16;
               memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
               buffer += 16;
            }
            *(uint8_t *) buffer = (flow[0].ip_version);
            buffer += 1;
            *(uint8_t *) buffer = (flow[0].ttl);
            buffer += 1;
            *(uint64_t *) buffer = ntohll(flow[0].bytes);
            buffer += 8;
            *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
            buffer += 8;
            *(uint8_t *) buffer = (flow[0].protocol);
            buffer += 1;
            *(uint16_t *) buffer = ntohs(flow[0].src_port);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(flow[0].dst_port);
            buffer += 2;
            *(uint8_t *) buffer = (flow[0].tcpflags);
            buffer += 1;
            buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
            buffer += 6;
            buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
            buffer += 6;
            *(uint8_t *) buffer = (ext[0].li);
            buffer += 0;
            *(uint8_t *) buffer = (ext[0].vn);
            buffer += 0;
            *(uint8_t *) buffer = (ext[0].mode);
            buffer += 0;
            *(uint8_t *) buffer = (ext[0].stratum);
            buffer += 1;
            *(uint8_t *) buffer = (ext[0].poll);
            buffer += 1;
            *(uint8_t *) buffer = (ext[0].precision);
            buffer += 1;
            *(uint32_t *) buffer = ntohl(ext[0].root_delay);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].root_dispersion);
            buffer += 4;
            *(uint32_t *) buffer = ntohl(ext[0].reference_id);
            buffer += 4;
            *(uint64_t *) buffer = ntohll(ext[0].reference_ts);
            buffer += 8;
            *(uint64_t *) buffer = ntohll(ext[0].origin_ts);
            buffer += 8;
            *(uint64_t *) buffer = ntohll(ext[0].receive_ts);
            buffer += 8;
            *(uint64_t *) buffer = ntohll(ext[0].transmit_ts);
            buffer += 8;
            tmplt->bufferSize = bufferSize;
            tmplt->recordCount++;
            break;
         }
         while (extRec->id == flow_ext_sip) {
            size_t str_len;
            struct sip_extension_s *ext = (struct sip_extension_s *) extRec->data;

            (void) str_len;
            (void) ext;

            if ((flow[0].ip_version) == (4)) {
               tmplt = ipfix->templateArray[10];
               while (tmplt->bufferSize + 89 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 89;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               *(uint32_t *) buffer = ntohl(flow[0].src_addr.v4.addr);
               buffer += 4;
               *(uint32_t *) buffer = ntohl(flow[0].dst_addr.v4.addr);
               buffer += 4;
            } else {
               tmplt = ipfix->templateArray[11];
               while (tmplt->bufferSize + 113 > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
               }
               buffer = tmplt->buffer + tmplt->bufferSize;
               bufferSize = tmplt->bufferSize + 113;
               *(uint16_t *) buffer = ntohs(ipfix->dir_bit_field);
               buffer += 2;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->first.tv_sec * 1000 + flow->first.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll((uint64_t) flow->last.tv_sec * 1000 + flow->last.tv_usec / 1000);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->id);
               buffer += 8;
               *(uint64_t *) buffer = ntohll(flow->parent);
               buffer += 8;
               memcpy(buffer, flow[0].src_addr.v6.addr, 16);
               buffer += 16;
               memcpy(buffer, flow[0].dst_addr.v6.addr, 16);
               buffer += 16;
            }
            *(uint8_t *) buffer = (flow[0].ip_version);
            buffer += 1;
            *(uint8_t *) buffer = (flow[0].ttl);
            buffer += 1;
            *(uint64_t *) buffer = ntohll(flow[0].bytes);
            buffer += 8;
            *(uint64_t *) buffer = ntohll((uint64_t)(flow[0].packets));
            buffer += 8;
            *(uint8_t *) buffer = (flow[0].protocol);
            buffer += 1;
            *(uint16_t *) buffer = ntohs(flow[0].src_port);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(flow[0].dst_port);
            buffer += 2;
            *(uint8_t *) buffer = (flow[0].tcpflags);
            buffer += 1;
            buffer[0] = (uint8_t) ((flow[0].src_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].src_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].src_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].src_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].src_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].src_hwaddr >> 0) & 0xFF);
            buffer += 6;
            buffer[0] = (uint8_t) ((flow[0].dst_hwaddr >> 40) & 0xFF);
            buffer[1] = (uint8_t) ((flow[0].dst_hwaddr >> 32) & 0xFF);
            buffer[2] = (uint8_t) ((flow[0].dst_hwaddr >> 24) & 0xFF);
            buffer[3] = (uint8_t) ((flow[0].dst_hwaddr >> 16) & 0xFF);
            buffer[4] = (uint8_t) ((flow[0].dst_hwaddr >> 8) & 0xFF);
            buffer[5] = (uint8_t) ((flow[0].dst_hwaddr >> 0) & 0xFF);
            buffer += 6;
            *(uint16_t *) buffer = ntohs(ext[0].msg_type);
            buffer += 2;
            *(uint16_t *) buffer = ntohs(ext[0].status_code);
            buffer += 2;
            str_len = strlen((const char *) ext[0].cseq);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].cseq, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].calling_party);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].calling_party, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].called_party);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].called_party, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].call_id);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].call_id, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].user_agent);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].user_agent, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].request_uri);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].request_uri, str_len);
            bufferSize += str_len;
            buffer += str_len;
            str_len = strlen((const char *) ext[0].via);
            if (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
               while (bufferSize + str_len > TEMPLATE_BUFFER_SIZE) {
                  ipfix_send_templates(ipfix);
                  ipfix_send_data(ipfix);
                  bufferSize = tmplt->bufferSize;
               }
               continue;
            }
            *buffer = str_len;
            buffer += 1;
            memcpy(buffer, ext[0].via, str_len);
            bufferSize += str_len;
            buffer += str_len;
            tmplt->bufferSize = bufferSize;
            tmplt->recordCount++;
            break;
         }

         extRec = extRec->next;
      }
   }

   return 0;
}

void ipfix_prepare(struct ipfix_s *ipfix)
{
   ipfix->templateArray = NULL;
   ipfix->templates = NULL;
   ipfix->templatesDataSize = 0;
   ipfix->verbose = 0;

   ipfix->sequenceNum = 0;
   ipfix->exportedPackets = 0;
   ipfix->fd = -1;
   ipfix->addrinfo = NULL;

   ipfix->host = "";
   ipfix->port = "";
   ipfix->protocol = IPPROTO_TCP;
   ipfix->ip = AF_UNSPEC; //AF_INET;
   ipfix->flags = 0;
   ipfix->reconnectTimeout = RECONNECT_TIMEOUT;
   ipfix->lastReconnect = 0;
   ipfix->odid = 0;
   ipfix->templateRefreshTime = TEMPLATE_REFRESH_TIME;
   ipfix->templateRefreshPackets = TEMPLATE_REFRESH_PACKETS;
   ipfix->export_basic = 1;
}

/**
 * \brief Exporter initialization
 *
 * @param params plugins Flowcache export plugins.
 * @param basic_num Index of basic pseudoplugin
 * @param odid Exporter identification
 * @param host Collector address
 * @param port Collector port
 * @param udp Use UDP instead of TCP
 * @return Returns 0 on succes, non 0 otherwise.
 */

int ipfix_init(struct ipfix_s *ipfix, uint32_t odid, const char *host, const char *port, int udp, int verbose, uint8_t dir, int export_basic)
{
   int ret;
   int templateCnt = 12;

   ipfix_prepare(ipfix);

   ipfix->verbose = verbose;
   if (ipfix->verbose) {
      fprintf(stderr, "VERBOSE: IPFIX export plugin init start\n");
   }

   ipfix->templateArray = (template_t **) malloc(templateCnt * sizeof(template_t *));
   for (int i = 0; i < templateCnt; i++) {
      ipfix->templateArray[i] = NULL;
   }
   ipfix->verbose = verbose;
   ipfix->host = host;
   ipfix->port = port;
   ipfix->odid = odid;
   ipfix->dir_bit_field = dir;
   ipfix->export_basic = export_basic;

   if (udp) {
      ipfix->protocol = IPPROTO_UDP;
   }

   ipfix->templateArray[0] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, NULL });
   ipfix->templateArray[1] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, NULL });
   ipfix->templateArray[2] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 16982, 100, -1 }, &(template_file_record_t){ 16982, 101, -1 }, &(template_file_record_t){ 16982, 102, -1 }, &(template_file_record_t){ 16982, 103, -1 }, &(template_file_record_t){ 16982, 105, -1 }, &(template_file_record_t){ 16982, 104, -1 }, &(template_file_record_t){ 16982, 106, 2 }, NULL });
   ipfix->templateArray[3] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 16982, 100, -1 }, &(template_file_record_t){ 16982, 101, -1 }, &(template_file_record_t){ 16982, 102, -1 }, &(template_file_record_t){ 16982, 103, -1 }, &(template_file_record_t){ 16982, 105, -1 }, &(template_file_record_t){ 16982, 104, -1 }, &(template_file_record_t){ 16982, 106, 2 }, NULL });
   ipfix->templateArray[4] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 810, 4 }, &(template_file_record_t){ 8057, 811, 4 }, &(template_file_record_t){ 8057, 812, 4 }, &(template_file_record_t){ 8057, 815, 4 }, &(template_file_record_t){ 8057, 816, 4 }, &(template_file_record_t){ 8057, 817, 4 }, &(template_file_record_t){ 8057, 818, 4 }, &(template_file_record_t){ 8057, 819, 4 }, &(template_file_record_t){ 8057, 820, -1 }, &(template_file_record_t){ 8057, 813, -1 }, &(template_file_record_t){ 8057, 814, -1 }, NULL });
   ipfix->templateArray[5] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 810, 4 }, &(template_file_record_t){ 8057, 811, 4 }, &(template_file_record_t){ 8057, 812, 4 }, &(template_file_record_t){ 8057, 815, 4 }, &(template_file_record_t){ 8057, 816, 4 }, &(template_file_record_t){ 8057, 817, 4 }, &(template_file_record_t){ 8057, 818, 4 }, &(template_file_record_t){ 8057, 819, 4 }, &(template_file_record_t){ 8057, 820, -1 }, &(template_file_record_t){ 8057, 813, -1 }, &(template_file_record_t){ 8057, 814, -1 }, NULL });
   ipfix->templateArray[6] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 808, -1 }, NULL });
   ipfix->templateArray[7] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 808, -1 }, NULL });
   ipfix->templateArray[8] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 18, 1 }, &(template_file_record_t){ 8057, 19, 1 }, &(template_file_record_t){ 8057, 20, 1 }, &(template_file_record_t){ 8057, 21, 1 }, &(template_file_record_t){ 8057, 22, 1 }, &(template_file_record_t){ 8057, 23, 1 }, &(template_file_record_t){ 8057, 24, 4 }, &(template_file_record_t){ 8057, 25, 4 }, &(template_file_record_t){ 8057, 26, 4 }, &(template_file_record_t){ 8057, 27, 8 }, &(template_file_record_t){ 8057, 28, 8 }, &(template_file_record_t){ 8057, 29, 8 }, &(template_file_record_t){ 8057, 30, 8 }, NULL });
   ipfix->templateArray[9] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 18, 1 }, &(template_file_record_t){ 8057, 19, 1 }, &(template_file_record_t){ 8057, 20, 1 }, &(template_file_record_t){ 8057, 21, 1 }, &(template_file_record_t){ 8057, 22, 1 }, &(template_file_record_t){ 8057, 23, 1 }, &(template_file_record_t){ 8057, 24, 4 }, &(template_file_record_t){ 8057, 25, 4 }, &(template_file_record_t){ 8057, 26, 4 }, &(template_file_record_t){ 8057, 27, 8 }, &(template_file_record_t){ 8057, 28, 8 }, &(template_file_record_t){ 8057, 29, 8 }, &(template_file_record_t){ 8057, 30, 8 }, NULL });
   ipfix->templateArray[10] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 8, 4 }, &(template_file_record_t){ 0, 12, 4 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 100, 2 }, &(template_file_record_t){ 8057, 101, 2 }, &(template_file_record_t){ 8057, 102, -1 }, &(template_file_record_t){ 8057, 103, -1 }, &(template_file_record_t){ 8057, 104, -1 }, &(template_file_record_t){ 8057, 105, -1 }, &(template_file_record_t){ 8057, 106, -1 }, &(template_file_record_t){ 8057, 107, -1 }, &(template_file_record_t){ 8057, 108, -1 }, NULL });
   ipfix->templateArray[11] = ipfix_create_template(ipfix, (const template_file_record_t *[]){ &(template_file_record_t){ 0, 10, 2 }, &(template_file_record_t){ 0, 152, 8 }, &(template_file_record_t){ 0, 153, 8 }, &(template_file_record_t){ 8057, 10000, 8 }, &(template_file_record_t){ 8057, 10001, 8 }, &(template_file_record_t){ 0, 27, 16 }, &(template_file_record_t){ 0, 28, 16 }, &(template_file_record_t){ 0, 60, 1 }, &(template_file_record_t){ 0, 192, 1 }, &(template_file_record_t){ 0, 1, 8 }, &(template_file_record_t){ 0, 2, 8 }, &(template_file_record_t){ 0, 4, 1 }, &(template_file_record_t){ 0, 7, 2 }, &(template_file_record_t){ 0, 11, 2 }, &(template_file_record_t){ 0, 6, 1 }, &(template_file_record_t){ 0, 56, 6 }, &(template_file_record_t){ 0, 80, 6 }, &(template_file_record_t){ 8057, 100, 2 }, &(template_file_record_t){ 8057, 101, 2 }, &(template_file_record_t){ 8057, 102, -1 }, &(template_file_record_t){ 8057, 103, -1 }, &(template_file_record_t){ 8057, 104, -1 }, &(template_file_record_t){ 8057, 105, -1 }, &(template_file_record_t){ 8057, 106, -1 }, &(template_file_record_t){ 8057, 107, -1 }, &(template_file_record_t){ 8057, 108, -1 }, NULL });

   ret = ipfix_connect_to_collector(ipfix);
   if (ret == 1) {
      return 1;
   } else if (ret == 2) {
      ipfix->lastReconnect = time(NULL);
   }

   if (ipfix->verbose) {
      fprintf(stderr, "VERBOSE: IPFIX export plugin init end\n");
   }
   return 0;
}

/**
 * \brief Initialise buffer for record with Data Set Header
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |          Set ID               |          Length               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * @param tmpl Template to init
 */
void ipfix_init_template_buffer(template_t *tmpl)
{
   *((uint16_t *) &tmpl->buffer[0]) = htons(tmpl->id);
   /* Length will be updated later */
   /* *((uint16_t *) &tmpl->buffer[2]) = htons(0); */
   tmpl->bufferSize = 4;
}

/**
 * \brief Fill ipfix template set header to memory specified by pointer
 *
 * @param ptr Pointer to memory to fill. Should be at least 4 bytes long
 * @param size Size of the template set including set header
 * @return size of the template set header
 */
int ipfix_fill_template_set_header(char *ptr, uint16_t size)
{
   ipfix_template_set_header_t *header = (ipfix_template_set_header_t *) ptr;

   header->id = htons(TEMPLATE_SET_ID);
   header->length = htons(size);

   return IPFIX_SET_HEADER_SIZE;
}

/**
 * \brief Check whether timeouts for template expired and set exported flag accordingly
 *
 * @param tmpl Template to check
 */
void ipfix_check_template_lifetime(struct ipfix_s *ipfix, template_t *tmpl)
{
   if (ipfix->templateRefreshTime != 0 &&
         (time_t) (ipfix->templateRefreshTime + tmpl->exportTime) <= time(NULL)) {
      if (ipfix->verbose) {
         fprintf(stderr, "VERBOSE: Template %i refresh time expired (%is)\n", tmpl->id, ipfix->templateRefreshTime);
      }
      tmpl->exported = 0;
   }

   if (ipfix->templateRefreshPackets != 0 &&
         ipfix->templateRefreshPackets + tmpl->exportPacket <= ipfix->exportedPackets) {
      if (ipfix->verbose) {
         fprintf(stderr, "VERBOSE: Template %i refresh packets expired (%i packets)\n", tmpl->id, ipfix->templateRefreshPackets);
      }
      tmpl->exported = 0;
   }
}

/**
 * \brief Fill ipfix header to memory specified by pointer
 *
 * @param ptr Pointer to memory to fill. Should be at least 16 bytes long
 * @param size Size of the IPFIX packet not including the header.
 * @return Returns size of the header
 */
int ipfix_fill_ipfix_header(struct ipfix_s *ipfix, char *ptr, uint16_t size)
{
   ipfix_header_t *header = (ipfix_header_t *)ptr;

   header->version = htons(IPFIX_VERISON);
   header->length = htons(size);
   header->exportTime = htonl(time(NULL));
   header->sequenceNumber = htonl(ipfix->sequenceNum);
   header->observationDomainId = htonl(ipfix->odid);

   return IPFIX_HEADER_SIZE;
}

/**
 * \brief Set all templates as expired
 */
void ipfix_expire_templates(struct ipfix_s *ipfix)
{
   template_t *tmp;
   for (tmp = ipfix->templates; tmp != NULL; tmp = tmp->next) {
      tmp->exported = 0;
      if (ipfix->protocol == IPPROTO_UDP) {
         tmp->exportTime = time(NULL);
         tmp->exportPacket = ipfix->exportedPackets;
      }
   }
}

/**
 * \brief Create new template based on given record
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |      Template ID (> 255)      |         Field Count           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * @param tmplt Template fields string
 * @param ext Template extension fields string
 * @return Created template on success, NULL otherwise
 */
template_t *ipfix_create_template(struct ipfix_s *ipfix, const template_file_record_t **tmplt)
{
   uint16_t maxID = FIRST_TEMPLATE_ID;
   uint16_t len;
   template_t *tmpTemplate = ipfix->templates;
   template_t *newTemplate;
   const template_file_record_t **tmp = tmplt;

   /* Create new template structure */
   newTemplate = (template_t *) malloc(sizeof(template_t));
   if (!newTemplate) {
      fprintf(stderr, "Error: Not enough memory for IPFIX template.\n");
      return NULL;
   }
   newTemplate->fieldCount = 0;
   newTemplate->recordCount = 0;

   /* Set template ID to maximum + 1 */
   while (tmpTemplate != NULL) {
      if (tmpTemplate->id >= maxID) maxID = tmpTemplate->id + 1;
      tmpTemplate = tmpTemplate->next;
   }
   newTemplate->id = maxID;
   ((uint16_t *) newTemplate->templateRecord)[0] = htons(newTemplate->id);

   if (ipfix->verbose) {
      fprintf(stderr, "VERBOSE: Creating new template id %u\n", newTemplate->id);
   }

   /* Template header size */
   newTemplate->templateSize = 4;

   while (tmp && *tmp) {
      /* Find appropriate template file record */
      const template_file_record_t *tmpFileRecord = *tmp;
      if (tmpFileRecord != NULL) {
         if (ipfix->verbose) {
            fprintf(stderr, "VERBOSE: Adding template field EN=%u ID=%u len=%d\n",
               tmpFileRecord->enterpriseNumber, tmpFileRecord->elementID, tmpFileRecord->length);
         }

         /* Set information element ID */
         uint16_t eID = tmpFileRecord->elementID;
         if (tmpFileRecord->enterpriseNumber != 0) {
            eID |= 0x8000;
         }
         *((uint16_t *) &newTemplate->templateRecord[newTemplate->templateSize]) = htons(eID);

         /* Set element length */
         if (tmpFileRecord->length == 0) {
            fprintf(stderr, "Error: Template field cannot be zero length.\n");
            free(newTemplate);
            return NULL;
         } else {
            len = tmpFileRecord->length;
         }
         *((uint16_t *) &newTemplate->templateRecord[newTemplate->templateSize + 2]) = htons(len);

         /* Update template size */
         newTemplate->templateSize += 4;

         /* Add enterprise number if required */
         if (tmpFileRecord->enterpriseNumber != 0) {
            *((uint32_t *) &newTemplate->templateRecord[newTemplate->templateSize]) =
               htonl(tmpFileRecord->enterpriseNumber);
            newTemplate->templateSize += 4;
         }

         /* Increase field count */
         newTemplate->fieldCount++;
      }

      tmp++;
   }

   /* Set field count */
   ((uint16_t *) newTemplate->templateRecord)[1] = htons(newTemplate->fieldCount);

   /* Initialize buffer for records */
   ipfix_init_template_buffer(newTemplate);

   /* Update total template size */
   ipfix->templatesDataSize += newTemplate->bufferSize;

   /* The template was not exported yet */
   newTemplate->exported = 0;
   newTemplate->exportTime = time(NULL);
   newTemplate->exportPacket = ipfix->exportedPackets;

   /* Add the new template to the list */
   newTemplate->next = ipfix->templates;
   ipfix->templates = newTemplate;

   return newTemplate;
}

/**
 * \brief Creates template packet
 *
 * Sets used templates as exported!
 *
 * @param packet Pointer to packet to fill
 * @return IPFIX packet with templates to export or NULL on failure
 */
uint16_t ipfix_create_template_packet(struct ipfix_s *ipfix, ipfix_packet_t *packet)
{
   template_t *tmp = ipfix->templates;
   uint16_t totalSize = 0;
   char *ptr;

   /* Get total size */
   while (tmp != NULL) {
      /* Check UDP template lifetime */
      if (ipfix->protocol == IPPROTO_UDP) {
         ipfix_check_template_lifetime(ipfix, tmp);
      }
      if (tmp->exported == 0) {
         totalSize += tmp->templateSize;
      }
      tmp = tmp->next;
   }

   /* Check that there are templates to export */
   if (totalSize == 0) {
      return 0;
   }

   totalSize += IPFIX_HEADER_SIZE + IPFIX_SET_HEADER_SIZE;

   /* Allocate memory for the packet */
   packet->data = (char *) malloc(sizeof(char)*(totalSize));
   if (!packet->data) {
      return 0;
   }
   ptr = packet->data;

   /* Create ipfix message header */
   ptr += ipfix_fill_ipfix_header(ipfix, ptr, totalSize);
   /* Create template set header */
   ptr += ipfix_fill_template_set_header(ptr, totalSize - IPFIX_HEADER_SIZE);


   /* Copy the templates to the packet */
   tmp = ipfix->templates;
   while (tmp != NULL) {
      if (tmp->exported == 0) {
         memcpy(ptr, tmp->templateRecord, tmp->templateSize);
         ptr += tmp->templateSize;
         /* Set the templates as exported, store time and serial number */
         tmp->exported = 1;
         tmp->exportTime = time(NULL);
         tmp->exportPacket = ipfix->exportedPackets;
      }
      tmp = tmp->next;
   }

   packet->length = totalSize;
   packet->flows = 0;

   return totalSize;
}

/**
 * \brief Creates data packet from template buffers
 *
 * Removes the data from the template buffers
 *
 * @param packet Pointer to packet to fill
 * @return length of the IPFIX data packet on success, 0 otherwise
 */
uint16_t ipfix_create_data_packet(struct ipfix_s *ipfix, ipfix_packet_t *packet)
{
   template_t *tmp = ipfix->templates;
   uint16_t totalSize = IPFIX_HEADER_SIZE; /* Include IPFIX header to total size */
   uint32_t deltaSequenceNum = 0; /* Number of exported records in this packet */
   char *ptr;

   /* Start adding data after the header */
   ptr = packet->data + totalSize;

   /* Copy the data sets to the packet */
   ipfix->templatesDataSize = 0; /* Erase total data size */
   while (tmp != NULL) {
      /* Add only templates with data that fits to one packet */
      if (tmp->recordCount > 0 && totalSize + tmp->bufferSize <= PACKET_DATA_SIZE) {
         memcpy(ptr, tmp->buffer, tmp->bufferSize);
         /* Set SET length */
         ((ipfix_template_set_header_t *) ptr)->length = htons(tmp->bufferSize);
         if (ipfix->verbose) {
            fprintf(stderr, "VERBOSE: Adding template %i of length %i to data packet\n", tmp->id, tmp->bufferSize);
         }
         ptr += tmp->bufferSize;
         /* Count size of the data copied to buffer */
         totalSize += tmp->bufferSize;
         /* Delete data from buffer */
         tmp->bufferSize = IPFIX_SET_HEADER_SIZE;

         /* Store number of exported records  */
         deltaSequenceNum += tmp->recordCount;
         tmp->recordCount = 0;
      }
      /* Update total data size, include empty template buffers (only set headers) */
      ipfix->templatesDataSize += tmp->bufferSize;
      tmp = tmp->next;
   }

   /* Check that there are packets to export */
   if (totalSize == IPFIX_HEADER_SIZE) {
      return 0;
   }

   /* Create ipfix message header at the beginning */
   //fill_ipfix_header(buff, totalSize);
   ipfix_fill_ipfix_header(ipfix, packet->data, totalSize);

   /* Fill number of flows and size of the packet */
   packet->flows = deltaSequenceNum;
   packet->length = totalSize;

   return totalSize;
}

/**
 * \brief Send all new templates to collector
 */
void ipfix_send_templates(struct ipfix_s *ipfix)
{
   ipfix_packet_t pkt;

   /* Send all new templates */
   if (ipfix_create_template_packet(ipfix, &pkt)) {
      /* Send template packet */
      /* After error, the plugin sends all templates after reconnection,
       * so we need not concern about it here */
      ipfix_send_packet(ipfix, &pkt);

      free(pkt.data);
   }
}

/**
 * \brief Send data in all buffers to collector
 */
void ipfix_send_data(struct ipfix_s *ipfix)
{
   char buffer[PACKET_DATA_SIZE];
   ipfix_packet_t pkt;
   pkt.data = buffer;

   /* Send all new templates */
   if (ipfix_create_data_packet(ipfix, &pkt)) {
      if (ipfix_send_packet(ipfix, &pkt) == 1) {
         /* Collector reconnected, resend the packet */
         ipfix_send_packet(ipfix, &pkt);
      }
   }
}

/**
 * \brief Export stored flows.
 */
void ipfix_flush(struct ipfix_s *ipfix)
{
   /* Send all new templates */
   ipfix_send_templates(ipfix);

   /* Send the data packet */
   ipfix_send_data(ipfix);
}

/**
 * \brief Sends packet using UDP or TCP as defined in plugin configuration
 *
 * When the collector disconnects, tries to reconnect and resend the data
 *
 * \param packet Packet to send
 * \return 0 on success, -1 on socket error, 1 when data needs to be resent (after reconnect)
 */
int ipfix_send_packet(struct ipfix_s *ipfix, ipfix_packet_t *packet)
{
   int ret; /* Return value of sendto */
   int sent = 0; /* Sent data size */

   /* Check that connection is OK or drop packet */
   if (ipfix_reconnect(ipfix)) {
      return -1;
   }

   /* sendto() does not guarantee that everything will be send in one piece */
   while (sent < packet->length) {
      /* Send data to collector (TCP and SCTP ignores last two arguments) */
      ret = sendto(ipfix->fd, (void *) (packet->data + sent), packet->length - sent, 0,
            ipfix->addrinfo->ai_addr, ipfix->addrinfo->ai_addrlen);

      /* Check that the data were sent correctly */
      if (ret == -1) {
         switch (errno) {
         case 0: break; /* OK */
         case ECONNRESET:
         case EINTR:
         case ENOTCONN:
         case ENOTSOCK:
         case EPIPE:
         case EHOSTUNREACH:
         case ENETDOWN:
         case ENETUNREACH:
         case ENOBUFS:
         case ENOMEM:

            /* The connection is broken */
            if (ipfix->verbose) {
               fprintf(stderr, "VERBOSE: Collector closed connection\n");
            }

            /* free resources */
            close(ipfix->fd);
            ipfix->fd = -1;
            freeaddrinfo(ipfix->addrinfo);

            /* Set last connection try time so that we would reconnect immediatelly */
            ipfix->lastReconnect = 1;

            /* Reset the sequences number since it is unique per connection */
            ipfix->sequenceNum = 0;
            ((ipfix_header_t *) packet->data)->sequenceNumber = 0; /* no need to change byteorder of 0 */

            /* Say that we should try to connect and send data again */
            return 1;
         default:
            /* Unknown error */
            if (ipfix->verbose) {
               perror("VERBOSE: Cannot send data to collector");
            }
            return -1;
         }
      }

      /* No error from sendto(), add sent data count to total */
      sent += ret;
   }

   /* Update sequence number for next packet */
   ipfix->sequenceNum += packet->flows;

   /* Increase packet counter */
   ipfix->exportedPackets++;

   if (ipfix->verbose) {
      fprintf(stderr, "VERBOSE: Packet (%" PRIu64 ") sent to %s on port %s. Next sequence number is %i\n",
            ipfix->exportedPackets, ipfix->host, ipfix->port, ipfix->sequenceNum);
   }

   return 0;
}

/**
 * \brief Create connection to collector
 *
 * The created socket is stored in conf->socket, addrinfo in conf->addrinfo
 * Addrinfo is freed up and socket is disconnected on error
 *
 * @return 0 on success, 1 on socket error or 2 when target is not listening
 */
int ipfix_connect_to_collector(struct ipfix_s *ipfix)
{
   struct addrinfo hints;
   struct addrinfo *tmp;
   int err;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family = ipfix->ip;
   hints.ai_protocol = ipfix->protocol;
   hints.ai_flags = AI_ADDRCONFIG | ipfix->flags;

   err = getaddrinfo(ipfix->host, ipfix->port, &hints, &ipfix->addrinfo);
   if (err) {
      if (err == EAI_SYSTEM) {
         fprintf(stderr, "Cannot get server info: %s\n", strerror(errno));
      } else {
         fprintf(stderr, "Cannot get server info: %s\n", gai_strerror(err));
      }
      return 1;
   }

   /* Try addrinfo strucutres one by one */
   for (tmp = ipfix->addrinfo; tmp != NULL; tmp = tmp->ai_next) {

      if (tmp->ai_family != AF_INET && tmp->ai_family != AF_INET6) {
         continue;
      }

      /* Print information about target address */
      char buff[INET6_ADDRSTRLEN];
      inet_ntop(tmp->ai_family,
            (tmp->ai_family == AF_INET) ?
                  (void *) &((struct sockaddr_in *) tmp->ai_addr)->sin_addr :
                  (void *) &((struct sockaddr_in6 *) tmp->ai_addr)->sin6_addr,
            (char *) &buff, sizeof(buff));

      if (ipfix->verbose) {
         fprintf(stderr, "VERBOSE: Connecting to IP %s\n", buff);
         fprintf(stderr, "VERBOSE: Socket configuration: AI Family: %i, AI Socktype: %i, AI Protocol: %i\n",
               tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
      }

      /* create socket */
      ipfix->fd = socket(ipfix->addrinfo->ai_family, ipfix->addrinfo->ai_socktype, ipfix->addrinfo->ai_protocol);
      if (ipfix->fd == -1) {
         if (ipfix->verbose) {
            perror("VERBOSE: Cannot create new socket");
         }
         continue;
      }

      /* connect to server with TCP and SCTP */
      if (ipfix->protocol != IPPROTO_UDP &&
            connect(ipfix->fd, ipfix->addrinfo->ai_addr, ipfix->addrinfo->ai_addrlen) == -1) {
         if (ipfix->verbose) {
            perror("VERBOSE: Cannot connect to collector");
         }
         close(ipfix->fd);
         ipfix->fd = -1;
         continue;
      }

      /* Connected, meaningless for UDP */
      if (ipfix->protocol != IPPROTO_UDP) {
         if (ipfix->verbose) {
            fprintf(stderr, "VERBOSE: Successfully connected to collector\n");
         }
      }
      break;
   }

   /* Return error when all addrinfo structures were tried*/
   if (tmp == NULL) {
      /* Free allocated resources */
      freeaddrinfo(ipfix->addrinfo);
      return 2;
   }

   return 0;
}

/**
 * \brief Checks that connection is OK or tries to reconnect
 *
 * @return 0 when connection is OK or reestablished, 1 when not
 */
int ipfix_reconnect(struct ipfix_s *ipfix)
{
   /* Check for broken connection */
   if (ipfix->lastReconnect != 0) {
      /* Check whether we need to attempt reconnection */
      if ((time_t) (ipfix->lastReconnect + ipfix->reconnectTimeout) <= time(NULL)) {
         /* Try to reconnect */
         if (ipfix_connect_to_collector(ipfix) == 0) {
            ipfix->lastReconnect = 0;
            /* Resend all templates */
            ipfix_expire_templates(ipfix);
            ipfix_send_templates(ipfix);
         } else {
            /* Set new reconnect time and drop packet */
            ipfix->lastReconnect = time(NULL);
            return 1;
         }
      } else {
         /* Timeout not reached, drop packet */
         return 1;
      }
   }

   return 0;
}
