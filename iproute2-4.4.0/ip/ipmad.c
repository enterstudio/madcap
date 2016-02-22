/* ipmad.c */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>

#include <linux/genetlink.h>
#include "utils.h"
#include "ip_common.h"
#include "rt_names.h"
#include "libgenl.h"


#include "../../include/madcap.h" /* XXX: need makefile magic */

/* netlink socket */
static struct rtnl_handle genl_rth;
static int genl_family = -1;

struct madcap_param {
	__u32 ifindex;
	__u16 offset;
	__u16 length;
	__u64 id;
	__u32 dst;

	int f_offset, f_length;	/* offset and length may become 0 correctly */
};

static void
usage (void)
{
	fprintf (stderr,
		 "usage:  ip mad { add | del } "
		 "[ id ID ] [ dst ADDR ] [ dev DEVICE ]\n"
		 "\n"
		 "        ip mad set "
		 "[ offset OFFSET ] [ length LENGTH ]\n"
		 "\n"
		 "        ip mad show { config }\n"
		);

	exit (-1);
}


static int
parse_args (int argc, char ** argv, struct madcap_param *p)
{
	if (argc < 1)
		usage ();

	memset (p, 0, sizeof (*p));

	while (argc > 0) {
		if (strcmp (*argv, "dev") == 0) {
			NEXT_ARG ();
			p->ifindex = if_nametoindex (*argv);
			if (!p->ifindex) {
				invarg ("invalid device", *argv);
				exit (-1);
			}
		} else if (strcmp (*argv, "offset") == 0) {
			NEXT_ARG ();
			if (get_u16 (&p->offset, *argv, 0)) {
				invarg ("invalid offset", *argv);
				exit (-1);
			}
			p->f_offset = 1;
		} else if (strcmp (*argv, "length") == 0) {
			NEXT_ARG ();
			if (get_u16 (&p->length, *argv, 0)) {
				invarg ("invalid length", *argv);
				exit (-1);
			}
			p->f_length = 1;
		} else if (strcmp (*argv, "id") == 0) {
			NEXT_ARG ();
			if (get_u64 (&p->id, *argv, 0)) {
				invarg ("invalid id", *argv);
				exit (-1);
			}
		} else if (strcmp (*argv, "dst") == 0) {
			NEXT_ARG ();
			if (inet_pton (AF_INET, *argv, &p->dst) < 1) {
				invarg ("invalid dst address", *argv);
				exit (-1);
			}
		}

		argc--;
		argv++;
	}

	return 0;
}

static int
do_add (int argc, char **argv)
{
	struct madcap_param p;
	struct madcap_obj_entry oe;

	parse_args (argc, argv, &p);

	if ((p.id == 0 && p.dst == 0) || p.ifindex == 0) {
		fprintf (stderr, "id, dst and dev must be specified\n");
		exit (-1);
	}

	memset (&oe, 0, sizeof (oe));
	oe.obj.id	= MADCAP_OBJ_ID_LLT_ENTRY;
	oe.obj.tb_id	= 0;	/* XXX */
	oe.id		= p.id;
	oe.dst		= p.dst;

	GENL_REQUEST (req, 1024, genl_family, 0, MADCAP_GENL_VERSION,
		      MADCAP_CMD_LLT_ENTRY_ADD, NLM_F_REQUEST | NLM_F_ACK);

	addattr32 (&req.n, 1024, MADCAP_ATTR_IFINDEX, p.ifindex);
	addattr_l (&req.n, 1024, MADCAP_ATTR_OBJ_ENTRY, &oe, sizeof (oe));

	if (rtnl_talk (&genl_rth, &req.n, NULL, 0) < 0)
		return -2;

	return 0;
}

static int
do_del (int argc, char **argv)
{
	struct madcap_param p;
	struct madcap_obj_entry oe;

	parse_args (argc, argv, &p);

	if ((p.id == 0 && p.dst == 0) || p.ifindex == 0) {
		fprintf (stderr, "id, dst and dev must be specified\n");
		exit (-1);
	}

	memset (&oe, 0, sizeof (oe));
	oe.obj.id	= MADCAP_OBJ_ID_LLT_ENTRY;
	oe.obj.tb_id	= 0;	/* XXX */
	oe.id		= p.id;
	oe.dst		= p.dst;

	GENL_REQUEST (req, 1024, genl_family, 0, MADCAP_GENL_VERSION,
		      MADCAP_CMD_LLT_ENTRY_DEL, NLM_F_REQUEST | NLM_F_ACK);

	addattr32 (&req.n, 1024, MADCAP_ATTR_IFINDEX, p.ifindex);
	addattr_l (&req.n, 1024, MADCAP_ATTR_OBJ_ENTRY, &oe, sizeof (oe));

	if (rtnl_talk (&genl_rth, &req.n, NULL, 0) < 0)
		return -2;

	return 0;
}

static int
do_set (int argc, char **argv)
{
	struct madcap_param p;
	struct madcap_obj_offset of;
	struct madcap_obj_length ol;

	parse_args (argc, argv, &p);

	GENL_REQUEST (req, 1024, genl_family, 0, MADCAP_GENL_VERSION,
		      MADCAP_CMD_LLT_CONFIG, NLM_F_REQUEST | NLM_F_ACK);

	if (p.f_offset) {
		/* config offset */
		memset (&of, 0, sizeof (of));
		of.obj.id	= MADCAP_OBJ_ID_LLT_OFFSET;
		of.obj.tb_id	= 0;	/* XXX */
		of.offset	= p.offset;
		addattr_l (&req.n, 1024, MADCAP_ATTR_OBJ_OFFSET,
			   &of, sizeof (of));
	}

	if (p.f_length) {
		/* config length */
		memset (&ol, 0, sizeof (ol));
		ol.obj.id	= MADCAP_OBJ_ID_LLT_LENGTH;
		ol.obj.tb_id	= 0;	/* XXX */
		ol.length	= p.length;
		addattr_l (&req.n, 1024, MADCAP_ATTR_OBJ_LENGTH,
			   &ol, sizeof (ol));
	}

	if (rtnl_talk (&genl_rth, &req.n, NULL, 0) < 0)
		return -2;

	return 0;
}

static int
obj_entry_nlmsg (const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	int len;
	char dev[IF_NAMESIZE] = { 0, };
	char dst[16] = { 0, };
	struct madcap_param *p = arg;
	struct genlmsghdr *ghdr;
	struct rtattr *attrs[MADCAP_ATTR_MAX + 1];
	struct madcap_obj_entry oe;
	
	if (!if_indextoname (p->ifindex, dev))
		return -1;

	ghdr = NLMSG_DATA (n);
	len = n->nlmsg_len - NLMSG_LENGTH (sizeof (*ghdr));
	if (len < 0)
		return -1;
	parse_rtattr (attrs, MADCAP_ATTR_MAX, (void *)ghdr + GENL_HDRLEN, len);

	if (!attrs[MADCAP_ATTR_OBJ_ENTRY])
		return -1;

	memcpy (&oe, RTA_DATA (attrs[MADCAP_ATTR_OBJ_ENTRY]), sizeof (oe));
	inet_ntop (AF_INET, &oe.dst, dst, sizeof (dst));
	
	fprintf (stdout, "id %x dst %s dev %s\n", oe.id, dst, dev);
	return 0;
}

static int
do_show (int argc, char **argv)
{
	int ret;
	struct madcap_param p;

	/* XXX: I have to know abount current usage of dump API.
	 * addattr to dump request can be handled in .dumpit ?
	 * show config ? or ip -d link show ?
	 */

	parse_args (argc, argv, &p);
	
	if (p.ifindex == 0) {
		fprintf (stderr, "devi must be specified\n");
		exit (-1);
	}

	GENL_REQUEST (req, 2048, genl_family, 0,
		      MADCAP_GENL_VERSION, MADCAP_CMD_LLT_ENTRY_GET,
		      NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST);

	addattr32 (&req.n, 1024, MADCAP_ATTR_IFINDEX, p.ifindex);
	req.n.nlmsg_seq = genl_rth.dump = ++genl_rth.seq;

	ret = rtnl_send (&genl_rth, &req.n, req.n.nlmsg_len);
	if (ret < 0) {
		fprintf (stderr, "%s:%d: error\n", __func__, __LINE__);
		return -2;
	}

	if (rtnl_dump_filter (&genl_rth, obj_entry_nlmsg, &p) < 0) {
		fprintf (stderr, "Dump terminated\n");
		exit (1);
	}

	return 0;
}

int
do_ipmad (int argc, char **argv)
{
	if (genl_family < 0) {
		if (rtnl_open_byproto (&genl_rth, 0, NETLINK_GENERIC) < 0) {
			fprintf (stderr, "Can't open genetlink socket\n");
			exit (1);
		}

		genl_family = genl_resolve_family (&genl_rth,
						   MADCAP_GENL_NAME);
		if (genl_family < 0)
			exit (1);
	}

	if (argc < 1)
		usage ();

	if (matches (*argv, "add") == 0)
		return do_add (argc - 1, argv + 1);
	if (matches (*argv, "del") == 0 || matches (*argv, "delete") == 0)
		return do_del (argc - 1, argv + 1);
	if (matches (*argv, "set") == 0)
		return do_set (argc - 1, argv + 1);
	if (matches (*argv, "show") == 0)
		return do_show (argc -1, argv + 1);
	if (matches (*argv, "help") == 0)
		usage ();
		
	fprintf (stderr, "Command \"%s\" is unknown, try \"ip mad help\".\n",
		 *argv);

	return -1;
}