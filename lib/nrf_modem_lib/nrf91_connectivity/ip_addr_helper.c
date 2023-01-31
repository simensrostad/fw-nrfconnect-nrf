#include <zephyr/device.h>
#include <zephyr/types.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <nrf_modem_at.h>
#include <string.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nrf91_connectivity, CONFIG_NRF91_CONNECTIVITY_LOG_LEVEL);

/* Current Ipv4 address. */
struct in_addr ipv4_addr_current;

static void ip_addr_get(int cid, char *addr4, char *addr6)
{
	int ret;
	char cmd[128];
	char tmp[sizeof(struct in6_addr)];
	char addr1[NET_IPV6_ADDR_LEN] = { 0 };
	char addr2[NET_IPV6_ADDR_LEN] = { 0 };

	sprintf(cmd, "AT+CGPADDR=%d", cid);
	/** parse +CGPADDR: <cid>,<PDP_addr_1>,<PDP_addr_2>
	 * PDN type "IP": PDP_addr_1 is <IPv4>, max 16(INET_ADDRSTRLEN), '.' and digits
	 * PDN type "IPV6": PDP_addr_1 is <IPv6>, max 46(INET6_ADDRSTRLEN),':', digits, 'A'~'F'
	 * PDN type "IPV4V6": <IPv4>,<IPv6> or <IPV4> or <IPv6>
	 */
	ret = nrf_modem_at_scanf(cmd, "+CGPADDR: %*d,\"%46[.:0-9A-F]\",\"%46[:0-9A-F]\"",
				 addr1, addr2);
	if (ret <= 0) {
		return;
	}
	if (addr4 != NULL && inet_pton(AF_INET, addr1, tmp) == 1) {
		strcpy(addr4, addr1);
	} else if (addr6 != NULL && inet_pton(AF_INET6, addr1, tmp) == 1) {
		strcpy(addr6, addr1);
		return;
	}
	/* parse second IP string, IPv6 only */
	if (addr6 == NULL) {
		return;
	}
	if (ret > 1 && inet_pton(AF_INET6, addr2, tmp) == 1) {
		strcpy(addr6, addr2);
	}
}

int ip_addr_add(const struct net_if *iface)
{
	int ret, len;
	char ipv4_addr[NET_IPV4_ADDR_LEN] = { 0 };
	char ipv6_addr[NET_IPV6_ADDR_LEN] = { 0 };
	struct sockaddr addr;
	struct net_if_addr *ifaddr;

	ip_addr_get(0, ipv4_addr, ipv6_addr);

	len = strlen(ipv4_addr);
	if (len == 0) {
		return -14;
	}

	LOG_ERR("ip ADDR: %s", ipv4_addr);

	ret = net_ipaddr_parse(ipv4_addr, len, &addr);
	if (!ret) {
		return -14;
	}

	ifaddr = net_if_ipv4_addr_add((struct net_if *)iface, &net_sin(&addr)->sin_addr, NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		return -14;
	}

	ipv4_addr_current = net_sin(&addr)->sin_addr;

	return 0;
}

int ip_addr_remove(const struct net_if *iface)
{
	if (!net_if_ipv4_addr_rm((struct net_if *)iface, &ipv4_addr_current)) {
		return -ENODEV;
	}

	return 0;
}
