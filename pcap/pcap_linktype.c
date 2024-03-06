/*
 * https://datatracker.ietf.org/doc/html/draft-richardson-opsawg-pcaplinktype-00
 */
#include "pcap_common.h"

#define _(X) case X: return #X

const char *pcap_get_linktype_name(uint32_t linktype)
{
	switch (linktype) {
		_(LINKTYPE_ETHERNET);
		_(LINKTYPE_IEEE802_5);
		_(LINKTYPE_SLIP);
		_(LINKTYPE_PPP);
		_(LINKTYPE_PPP_HDLC);
		_(LINKTYPE_PPP_ETHER);
		_(LINKTYPE_IEEE802_11);
		_(LINKTYPE_LOOP);
		_(LINKTYPE_HDLC);
		_(LINKTYPE_PPP_PPPD);
		_(LINKTYPE_USB_LINUX);
		_(LINKTYPE_IPV4);
		_(LINKTYPE_IPV6);
		_(LINKTYPE_IEEE802_15_4_NOFCS);
		_(LINKTYPE_DBUS);
		_(LINKTYPE_MUX27010);
		_(LINKTYPE_USBPCAP);
	};

	return NULL;
}
