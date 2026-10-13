#include "iface_gre.h"
#include "softgred_config.h"
static void test_attributes(void)
{
    struct link_request req;
    link_request_init(&req, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL, 0);
    g_assert_cmpuint(req.header.nlmsg_len, ==, NLMSG_LENGTH(sizeof(struct ifinfomsg)));
    const char *name = "sgre0";
    g_assert_cmpint(link_attr(&req, IFLA_IFNAME, name, strlen(name) + 1), ==, 0);
    struct rtattr *info = link_nest(&req, IFLA_LINKINFO);
    g_assert_nonnull(info);
    g_assert_cmpint(link_attr(&req, IFLA_INFO_KIND, "gretap", 7), ==, 0);
    struct rtattr *data = link_nest(&req, IFLA_INFO_DATA);
    uint32_t address = htonl(0xc0000201);
    g_assert_cmpint(link_attr(&req, IFLA_GRE_LOCAL, &address, 4), ==, 0);
    link_nest_end(&req, data); link_nest_end(&req, info);
    int len = req.header.nlmsg_len - NLMSG_LENGTH(sizeof(req.info));
    struct rtattr *attr = IFLA_RTA(&req.info);
    g_assert_true(RTA_OK(attr, len));
    g_assert_cmpstr(RTA_DATA(attr), ==, name);
    g_assert_cmpuint(RTA_PAYLOAD(attr), ==, strlen(name) + 1);
    attr = RTA_NEXT(attr, len);
    g_assert_true(RTA_OK(attr, len));
    g_assert_cmpuint(attr->rta_type, ==, IFLA_LINKINFO | NLA_F_NESTED);
    unsigned before = req.header.nlmsg_len;
    g_assert_cmpint(link_attr(&req, IFLA_IFNAME, name, sizeof(req)), ==, -1);
    g_assert_cmpint(errno, ==, EMSGSIZE);
    g_assert_cmpuint(req.header.nlmsg_len, ==, before);
}
static void test_no_interface(void)
{
    g_assert_cmpint(iface_gre_del("sg-test-absent"), ==, -1);
    g_assert_cmpint(errno, ==, ENODEV);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/netlink/attributes-and-bounds", test_attributes);
    g_test_add_func("/netlink/absent-interface", test_no_interface);
    return g_test_run();
}
