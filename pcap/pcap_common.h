#ifndef PCAP_COMMON_H
#define PCAP_COMMON_H

#include <stdio.h>
#include <stdint.h>

#define BLOCK_TYPE_SHB				0x0A0D0D0A /* Section Header Block */
#define BLOCK_TYPE_IDB				0x00000001 /* Interface Description Block */
#define BLOCK_TYPE_PB				0x00000002 /* Packet Block (obsolete) */
#define BLOCK_TYPE_SPB				0x00000003 /* Simple Packet Block */
#define BLOCK_TYPE_NRB				0x00000004 /* Name Resolution Block */
#define BLOCK_TYPE_ISB				0x00000005 /* Interface Statistics Block */
#define BLOCK_TYPE_EPB				0x00000006 /* Enhanced Packet Block */
#define BLOCK_TYPE_IRIG_TS			0x00000007 /* IRIG Timestamp Block */
#define BLOCK_TYPE_ARINC_429			0x00000008 /* ARINC 429 in AFDX Encapsulation Information Block */
#define BLOCK_TYPE_SYSTEMD_JOURNAL_EXPORT	0x00000009 /* systemd journal entry */
#define BLOCK_TYPE_DSB				0x0000000A /* Decryption Secrets Block */
#define BLOCK_TYPE_SYSDIG_EVENT			0x00000204 /* Sysdig Event Block */
#define BLOCK_TYPE_SYSDIG_EVF			0x00000208 /* Sysdig Event Block with flags */
#define BLOCK_TYPE_SYSDIG_EVENT_V2		0x00000216 /* Sysdig Event Block version 2 */
#define BLOCK_TYPE_SYSDIG_EVF_V2		0x00000217 /* Sysdig Event Block with flags version 2 */
#define BLOCK_TYPE_SYSDIG_EVENT_V2_LARGE	0x00000221 /* Sysdig Event Block version 2 with large payload */
#define BLOCK_TYPE_SYSDIG_EVF_V2_LARGE		0x00000222 /* Sysdig Event Block with flags version 2 with large payload */
#define BLOCK_TYPE_CB_COPY			0x00000BAD /* Custom Block which can be copied */
#define BLOCK_TYPE_CB_NO_COPY			0x40000BAD /* Custom Block which should not be copied */

#define LINKTYPE_ETHERNET			1
#define LINKTYPE_IEEE802_5			6
#define LINKTYPE_SLIP				8
#define LINKTYPE_PPP				9
#define LINKTYPE_PPP_HDLC			50
#define LINKTYPE_PPP_ETHER			51
#define LINKTYPE_IEEE802_11			105
#define LINKTYPE_LOOP				108
#define LINKTYPE_HDLC				112
#define LINKTYPE_PPP_PPPD			166
#define LINKTYPE_USB_LINUX			189
#define LINKTYPE_IPV4				228
#define LINKTYPE_IPV6				229
#define LINKTYPE_IEEE802_15_4_NOFCS		230
#define LINKTYPE_DBUS				231
#define LINKTYPE_MUX27010			236
#define LINKTYPE_USBPCAP			249

const char *pcap_get_linktype_name(uint32_t linktype);

struct pcap_option {
	uint16_t type;
	uint16_t value_length;
};
#define OPT_ENDOFOPT				0
#define OPT_COMMENT				1
#define OPT_EPB_FLAGS				2
#define OPT_SHB_HARDWARE			2 /* currently not used */
#define OPT_SHB_OS				3
#define OPT_SHB_USERAPPL			4
#define OPT_IDB_NAME				2
#define OPT_IDB_DESCRIPTION			3
#define OPT_IDB_IF_SPEED			8
#define OPT_IDB_TSRESOL				9
#define OPT_IDB_FILTER				11
#define OPT_IDB_OS				12
#define OPT_IDB_HARDWARE			15
#define OPT_ISB_STARTTIME			2
#define OPT_ISB_ENDTIME				3
#define OPT_ISB_IFRECV				4
#define OPT_ISB_IFDROP				5
#define OPT_ISB_FILTERACCEPT			6
#define OPT_ISB_OSDROP				7
#define OPT_ISB_USRDELIV			8
#define ADD_PADDING(x)				((((x) + 3) >> 2) << 2)

#endif
