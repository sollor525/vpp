/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * interface_output.c: interface output node
 *
 * Copyright (c) 2008 Eliot Dresselhaus
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __INTERFACE_INLINES_H__
#define __INTERFACE_INLINES_H__

#include <vnet/vnet.h>
#include <vnet/tcp/tcp_packet.h>

static_always_inline void
vnet_calc_ip4_checksums (vlib_main_t *vm, vlib_buffer_t *b, ip4_header_t *ip4,
			 tcp_header_t *th, udp_header_t *uh,
			 vnet_buffer_oflags_t oflags)
{
  if (oflags & VNET_BUFFER_OFFLOAD_F_IP_CKSUM)
    ip4->checksum = ip4_header_checksum (ip4);
  if (oflags & VNET_BUFFER_OFFLOAD_F_TCP_CKSUM)
    {
      th->checksum = 0;
      th->checksum = ip4_tcp_udp_compute_checksum (vm, b, ip4);
    }
  if (oflags & VNET_BUFFER_OFFLOAD_F_UDP_CKSUM)
    {
      uh->checksum = 0;
      uh->checksum = ip4_tcp_udp_compute_checksum (vm, b, ip4);
    }
}

static_always_inline void
vnet_calc_ip6_checksums (vlib_main_t *vm, vlib_buffer_t *b, ip6_header_t *ip6,
			 tcp_header_t *th, udp_header_t *uh,
			 vnet_buffer_oflags_t oflags)
{
  int bogus;
  if (oflags & VNET_BUFFER_OFFLOAD_F_TCP_CKSUM)
    {
      th->checksum = 0;
      th->checksum = ip6_tcp_udp_icmp_compute_checksum (vm, b, ip6, &bogus);
    }
  if (oflags & VNET_BUFFER_OFFLOAD_F_UDP_CKSUM)
    {
      uh->checksum = 0;
      uh->checksum = ip6_tcp_udp_icmp_compute_checksum (vm, b, ip6, &bogus);
    }
}

static_always_inline void
vnet_calc_checksums_inline (vlib_main_t * vm, vlib_buffer_t * b,
			    int is_ip4, int is_ip6)
{
  ip4_header_t *ip4;
  ip6_header_t *ip6;
  tcp_header_t *th;
  udp_header_t *uh;
  vnet_buffer_oflags_t oflags;

  if (!(b->flags & VNET_BUFFER_F_OFFLOAD))
    return;

  ASSERT (!(is_ip4 && is_ip6));

  ip4 = (ip4_header_t *) (b->data + vnet_buffer (b)->l3_hdr_offset);
  ip6 = (ip6_header_t *) (b->data + vnet_buffer (b)->l3_hdr_offset);
  th = (tcp_header_t *) (b->data + vnet_buffer (b)->l4_hdr_offset);
  uh = (udp_header_t *) (b->data + vnet_buffer (b)->l4_hdr_offset);
  oflags = vnet_buffer (b)->oflags;

  if (is_ip4)
    {
      vnet_calc_ip4_checksums (vm, b, ip4, th, uh, oflags);
    }
  else if (is_ip6)
    {
      vnet_calc_ip6_checksums (vm, b, ip6, th, uh, oflags);
    }

  vnet_buffer_offload_flags_clear (b, (VNET_BUFFER_OFFLOAD_F_IP_CKSUM |
				       VNET_BUFFER_OFFLOAD_F_UDP_CKSUM |
				       VNET_BUFFER_OFFLOAD_F_TCP_CKSUM));
}

static_always_inline void
vnet_calc_outer_checksums_inline (vlib_main_t *vm, vlib_buffer_t *b)
{

  if (!(b->flags & VNET_BUFFER_F_OFFLOAD))
    return;

  vnet_buffer_oflags_t oflags = vnet_buffer (b)->oflags;
  if (oflags & VNET_BUFFER_OFFLOAD_F_OUTER_IP_CKSUM)
    {
      ip4_header_t *ip4;
      ip4 = (ip4_header_t *) (b->data + vnet_buffer2 (b)->outer_l3_hdr_offset);
      ip4->checksum = ip4_header_checksum (ip4);
      vnet_buffer_offload_flags_clear (b,
				       VNET_BUFFER_OFFLOAD_F_OUTER_IP_CKSUM);
    }
  else if (oflags & VNET_BUFFER_OFFLOAD_F_OUTER_UDP_CKSUM)
    {
      int bogus;
      ip6_header_t *ip6;
      udp_header_t *uh;

      ip6 = (ip6_header_t *) (b->data + vnet_buffer2 (b)->outer_l3_hdr_offset);
      uh = (udp_header_t *) (b->data + vnet_buffer2 (b)->outer_l4_hdr_offset);
      uh->checksum = 0;
      uh->checksum = ip6_tcp_udp_icmp_compute_checksum (vm, b, ip6, &bogus);
      vnet_buffer_offload_flags_clear (b,
				       VNET_BUFFER_OFFLOAD_F_OUTER_UDP_CKSUM);
    }
  vnet_buffer_offload_flags_clear (b, VNET_BUFFER_OFFLOAD_F_TNL_MASK);
}
#endif

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
