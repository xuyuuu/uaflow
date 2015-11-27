#ifndef flow_core_types_h
#define flow_core_types_h

#define FLOW_PMD_MODE_RX 0
#define FLOW_PMD_MODE_TX 1

#define FLOW_LCORE_MAX_SLOT 32

enum {
  FLOW_IPPROTO_IP = 0,		/* Dummy protocol for TCP		*/
#define FLOW_IPPROTO_IP		FLOW_IPPROTO_IP
  FLOW_IPPROTO_ICMP = 1,		/* Internet Control Message Protocol	*/
#define FLOW_IPPROTO_ICMP		FLOW_IPPROTO_ICMP
  FLOW_IPPROTO_IGMP = 2,		/* Internet Group Management Protocol	*/
#define FLOW_IPPROTO_IGMP		FLOW_IPPROTO_IGMP
  FLOW_IPPROTO_IPIP = 4,		/* IPIP tunnels (older KA9Q tunnels use 94) */
#define FLOW_IPPROTO_IPIP		FLOW_IPPROTO_IPIP
  FLOW_IPPROTO_TCP = 6,		/* Transmission Control Protocol	*/
#define FLOW_IPPROTO_TCP		FLOW_IPPROTO_TCP
  FLOW_IPPROTO_EGP = 8,		/* Exterior Gateway Protocol		*/
#define FLOW_IPPROTO_EGP		FLOW_IPPROTO_EGP
  FLOW_IPPROTO_PUP = 12,		/* PUP protocol				*/
#define FLOW_IPPROTO_PUP		FLOW_IPPROTO_PUP
  FLOW_IPPROTO_UDP = 17,		/* User Datagram Protocol		*/
#define FLOW_IPPROTO_UDP		FLOW_IPPROTO_UDP
  FLOW_IPPROTO_IDP = 22,		/* XNS IDP protocol			*/
#define FLOW_IPPROTO_IDP		FLOW_IPPROTO_IDP
  FLOW_IPPROTO_TP = 29,		/* SO Transport Protocol Class 4	*/
#define FLOW_IPPROTO_TP		FLOW_IPPROTO_TP
  FLOW_IPPROTO_DCCP = 33,		/* Datagram Congestion Control Protocol */
#define FLOW_IPPROTO_DCCP		FLOW_IPPROTO_DCCP
  FLOW_IPPROTO_IPV6 = 41,		/* IPv6-in-IPv4 tunnelling		*/
#define FLOW_IPPROTO_IPV6		FLOW_IPPROTO_IPV6
  FLOW_IPPROTO_RSVP = 46,		/* RSVP Protocol			*/
#define FLOW_IPPROTO_RSVP		FLOW_IPPROTO_RSVP
  FLOW_IPPROTO_GRE = 47,		/* Cisco GRE tunnels (rfc 1701,1702)	*/
#define FLOW_IPPROTO_GRE		FLOW_IPPROTO_GRE
  FLOW_IPPROTO_ESP = 50,		/* Encapsulation Security Payload protocol */
#define FLOW_IPPROTO_ESP		FLOW_IPPROTO_ESP
  FLOW_IPPROTO_AH = 51,		/* Authentication Header protocol	*/
#define FLOW_IPPROTO_AH		FLOW_IPPROTO_AH
  FLOW_IPPROTO_MTP = 92,		/* Multicast Transport Protocol		*/
#define FLOW_IPPROTO_MTP		FLOW_IPPROTO_MTP
  FLOW_IPPROTO_BEETPH = 94,		/* IP option pseudo header for BEET	*/
#define FLOW_IPPROTO_BEETPH		FLOW_IPPROTO_BEETPH
  FLOW_IPPROTO_ENCAP = 98,		/* Encapsulation Header			*/
#define FLOW_IPPROTO_ENCAP		FLOW_IPPROTO_ENCAP
  FLOW_IPPROTO_PIM = 103,		/* Protocol Independent Multicast	*/
#define FLOW_IPPROTO_PIM		FLOW_IPPROTO_PIM
  FLOW_IPPROTO_COMP = 108,		/* Compression Header Protocol		*/
#define FLOW_IPPROTO_COMP		FLOW_IPPROTO_COMP
  FLOW_IPPROTO_SCTP = 132,		/* Stream Control Transport Protocol	*/
#define FLOW_IPPROTO_SCTP		FLOW_IPPROTO_SCTP
  FLOW_IPPROTO_UDPLITE = 136,	/* UDP-Lite (RFC 3828)			*/
#define FLOW_IPPROTO_UDPLITE		FLOW_IPPROTO_UDPLITE
  FLOW_IPPROTO_RAW = 255,		/* Raw IP packets			*/
#define FLOW_IPPROTO_RAW		FLOW_IPPROTO_RAW
  FLOW_IPPROTO_MAX
};

#define FLOW_IP_TOS		1
#define FLOW_IP_TTL		2
#define FLOW_IP_HDRINCL	3
#define FLOW_IP_OPTIONS	4
#define FLOW_IP_ROUTER_ALERT	5
#define FLOW_IP_RECVOPTS	6
#define FLOW_IP_RETOPTS	7
#define FLOW_IP_PKTINFO	8
#define FLOW_IP_PKTOPTIONS	9
#define FLOW_IP_MTU_DISCOVER	10
#define FLOW_IP_RECVERR	11
#define FLOW_IP_RECVTTL	12
#define	FLOW_IP_RECVTOS	13
#define FLOW_IP_MTU		14
#define FLOW_IP_FREEBIND	15
#define FLOW_IP_IPSEC_POLICY	16
#define FLOW_IP_XFRM_POLICY	17
#define FLOW_IP_PASSSEC	18
#define FLOW_IP_TRANSPARENT	19
/* BSD compatibility */
#define FLOW_IP_RECVRETOPTS	FLOW_IP_RETOPTS

/* TProxy original addresses */
#define FLOW_IP_ORIGDSTADDR       20
#define FLOW_IP_RECVORIGDSTADDR   FLOW_IP_ORIGDSTADDR

#define FLOW_IP_MINTTL       21

/* FLOW_IP_MTU_DISCOVER values */
#define FLOW_IP_PMTUDISC_DONT		0	/* Never send DF frames */
#define FLOW_IP_PMTUDISC_WANT		1	/* Use per route hints	*/
#define FLOW_IP_PMTUDISC_DO			2	/* Always DF		*/
#define FLOW_IP_PMTUDISC_PROBE		3       /* Ignore dst pmtu      */

#define FLOW_IP_PMTUDISC_INTERFACE           4
/* weaker version of FLOW_IP_PMTUDISC_INTERFACE, which allos packets to get
 * fragmented if they exeed the interface mtu
 */
#define FLOW_IP_PMTUDISC_OMIT                5

#define FLOW_IP_MULTICAST_IF			32
#define FLOW_IP_MULTICAST_TTL 		33
#define FLOW_IP_MULTICAST_LOOP 		34
#define FLOW_IP_ADD_MEMBERSHIP		35
#define FLOW_IP_DROP_MEMBERSHIP		36
#define FLOW_IP_UNBLOCK_SOURCE		37
#define FLOW_IP_BLOCK_SOURCE			38
#define FLOW_IP_ADD_SOURCE_MEMBERSHIP	39
#define FLOW_IP_DROP_SOURCE_MEMBERSHIP	40
#define FLOW_IP_MSFILTER			41
#define FLOW_MCAST_JOIN_GROUP		42
#define FLOW_MCAST_BLOCK_SOURCE		43
#define FLOW_MCAST_UNBLOCK_SOURCE		44
#define FLOW_MCAST_LEAVE_GROUP		45
#define FLOW_MCAST_JOIN_SOURCE_GROUP		46
#define FLOW_MCAST_LEAVE_SOURCE_GROUP	47
#define FLOW_MCAST_MSFILTER			48
#define FLOW_IP_MULTICAST_ALL		49

#define FLOW_MCAST_EXCLUDE	0
#define FLOW_MCAST_INCLUDE	1

/* These need to appear somewhere around here */
#define FLOW_IP_DEFAULT_MULTICAST_TTL        1




#endif
