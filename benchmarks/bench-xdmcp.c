/*
 * LightDM Performance Benchmarks
 * Suite: XDMCP Protocol Serialization (bench-xdmcp.c)
 */

#include "benchmark-framework.h"
#include "xdmcp-protocol.h"
#include <string.h>

typedef struct {
    XDMCPPacket *request_packet;
    guchar request_raw[1024];
    gssize request_raw_len;

    XDMCPPacket *manage_packet;
    guchar manage_raw[1024];
    gssize manage_raw_len;

    XDMCPPacket *accept_packet;
    guchar accept_raw[1024];
    gssize accept_raw_len;
} XDMCPSuiteFixture;

static void
xdmcp_setup (gpointer *fixture_out)
{
    XDMCPSuiteFixture *fix = g_new0 (XDMCPSuiteFixture, 1);

    /* 1. Build sample Request packet */
    fix->request_packet = xdmcp_packet_alloc (XDMCP_Request);
    fix->request_packet->Request.display_number = 1;
    fix->request_packet->Request.n_connections = 2;
    fix->request_packet->Request.connections = g_new0 (XDMCPConnection, 2);

    const guint8 addr1[] = { 192, 168, 1, 50 };
    fix->request_packet->Request.connections[0].type = 0; /* TCP */
    fix->request_packet->Request.connections[0].address.length = sizeof (addr1);
    fix->request_packet->Request.connections[0].address.data = g_memdup2 (addr1, sizeof (addr1));

    const guint8 addr2[] = { 127, 0, 0, 1 };
    fix->request_packet->Request.connections[1].type = 0;
    fix->request_packet->Request.connections[1].address.length = sizeof (addr2);
    fix->request_packet->Request.connections[1].address.data = g_memdup2 (addr2, sizeof (addr2));

    fix->request_packet->Request.authentication_name = g_strdup ("XDM-AUTHENTICATION-1");
    const guint8 auth_data[] = "sample-secret-token-1234";
    fix->request_packet->Request.authentication_data.length = strlen ((char *) auth_data);
    fix->request_packet->Request.authentication_data.data = g_memdup2 (auth_data, strlen ((char *) auth_data));

    fix->request_packet->Request.authorization_names = g_new0 (gchar *, 3);
    fix->request_packet->Request.authorization_names[0] = g_strdup ("MIT-MAGIC-COOKIE-1");
    fix->request_packet->Request.authorization_names[1] = g_strdup ("XDM-AUTHORIZATION-1");
    fix->request_packet->Request.authorization_names[2] = NULL;

    fix->request_packet->Request.manufacturer_display_id = g_strdup ("LightDM-Bench-Display-01");

    fix->request_raw_len = xdmcp_packet_encode (fix->request_packet, fix->request_raw, sizeof (fix->request_raw));

    /* 2. Build sample Manage packet */
    fix->manage_packet = xdmcp_packet_alloc (XDMCP_Manage);
    fix->manage_packet->Manage.session_id = 987654321;
    fix->manage_packet->Manage.display_number = 1;
    fix->manage_packet->Manage.display_class = g_strdup ("MIT-unspecified");
    fix->manage_raw_len = xdmcp_packet_encode (fix->manage_packet, fix->manage_raw, sizeof (fix->manage_raw));

    /* 3. Build sample Accept packet */
    fix->accept_packet = xdmcp_packet_alloc (XDMCP_Accept);
    fix->accept_packet->Accept.session_id = 987654321;
    fix->accept_packet->Accept.authentication_name = g_strdup ("XDM-AUTHENTICATION-1");
    fix->accept_packet->Accept.authentication_data.length = 8;
    fix->accept_packet->Accept.authentication_data.data = g_memdup2 ("12345678", 8);
    fix->accept_packet->Accept.authorization_name = g_strdup ("MIT-MAGIC-COOKIE-1");
    fix->accept_packet->Accept.authorization_data.length = 16;
    fix->accept_packet->Accept.authorization_data.data = g_memdup2 ("0123456789abcdef", 16);
    fix->accept_raw_len = xdmcp_packet_encode (fix->accept_packet, fix->accept_raw, sizeof (fix->accept_raw));

    *fixture_out = fix;
}

static void
xdmcp_teardown (gpointer fixture)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;
    if (!fix) return;

    if (fix->request_packet)
        xdmcp_packet_free (fix->request_packet);
    if (fix->manage_packet)
        xdmcp_packet_free (fix->manage_packet);
    if (fix->accept_packet)
        xdmcp_packet_free (fix->accept_packet);

    g_free (fix);
}

/* Benchmark: Encode XDMCP_Request */
static void
bench_encode_request (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;
    guchar buffer[1024];

    for (guint64 op = 0; op < inner_ops; op++)
    {
        gssize len = xdmcp_packet_encode (fix->request_packet, buffer, sizeof (buffer));
        (void) len;
    }
}

/* Benchmark: Decode XDMCP_Request */
static void
bench_decode_request (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        XDMCPPacket *packet = xdmcp_packet_decode (fix->request_raw, fix->request_raw_len);
        xdmcp_packet_free (packet);
    }
}

/* Benchmark: Encode XDMCP_Manage */
static void
bench_encode_manage (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;
    guchar buffer[1024];

    for (guint64 op = 0; op < inner_ops; op++)
    {
        gssize len = xdmcp_packet_encode (fix->manage_packet, buffer, sizeof (buffer));
        (void) len;
    }
}

/* Benchmark: Decode XDMCP_Manage */
static void
bench_decode_manage (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        XDMCPPacket *packet = xdmcp_packet_decode (fix->manage_raw, fix->manage_raw_len);
        xdmcp_packet_free (packet);
    }
}

/* Benchmark: Encode XDMCP_Accept */
static void
bench_encode_accept (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;
    guchar buffer[1024];

    for (guint64 op = 0; op < inner_ops; op++)
    {
        gssize len = xdmcp_packet_encode (fix->accept_packet, buffer, sizeof (buffer));
        (void) len;
    }
}

/* Benchmark: Decode XDMCP_Accept */
static void
bench_decode_accept (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        XDMCPPacket *packet = xdmcp_packet_decode (fix->accept_raw, fix->accept_raw_len);
        xdmcp_packet_free (packet);
    }
}

/* Benchmark: Full Round-trip (encode -> decode -> free) */
static void
bench_roundtrip (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;
    guchar buffer[1024];

    for (guint64 op = 0; op < inner_ops; op++)
    {
        gssize len = xdmcp_packet_encode (fix->request_packet, buffer, sizeof (buffer));
        XDMCPPacket *packet = xdmcp_packet_decode (buffer, len);
        xdmcp_packet_free (packet);
    }
}

/* Benchmark: Packet toString serialization (for debugging/logging) */
static void
bench_tostring (gpointer fixture, guint64 inner_ops)
{
    XDMCPSuiteFixture *fix = (XDMCPSuiteFixture *) fixture;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        gchar *str = xdmcp_packet_tostring (fix->request_packet);
        g_free (str);
    }
}

void
bench_xdmcp_register (void)
{
    static const BenchmarkDef b1 = {
        .suite = "xdmcp",
        .name = "encode_request",
        .description = "Serialize complex XDMCP_Request packet to binary buffer",
        .setup = xdmcp_setup,
        .run = bench_encode_request,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b1);

    static const BenchmarkDef b2 = {
        .suite = "xdmcp",
        .name = "decode_request",
        .description = "Deserialize raw bytes to XDMCP_Request packet tree",
        .setup = xdmcp_setup,
        .run = bench_decode_request,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b2);

    static const BenchmarkDef b3 = {
        .suite = "xdmcp",
        .name = "encode_manage",
        .description = "Serialize XDMCP_Manage packet to binary buffer",
        .setup = xdmcp_setup,
        .run = bench_encode_manage,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b3);

    static const BenchmarkDef b4 = {
        .suite = "xdmcp",
        .name = "decode_manage",
        .description = "Deserialize raw bytes to XDMCP_Manage packet",
        .setup = xdmcp_setup,
        .run = bench_decode_manage,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b4);

    static const BenchmarkDef b5 = {
        .suite = "xdmcp",
        .name = "encode_accept",
        .description = "Serialize XDMCP_Accept packet with authentication data",
        .setup = xdmcp_setup,
        .run = bench_encode_accept,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b5);

    static const BenchmarkDef b6 = {
        .suite = "xdmcp",
        .name = "decode_accept",
        .description = "Deserialize raw bytes to XDMCP_Accept packet",
        .setup = xdmcp_setup,
        .run = bench_decode_accept,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b6);

    static const BenchmarkDef b7 = {
        .suite = "xdmcp",
        .name = "roundtrip_request",
        .description = "Full encode -> decode -> free roundtrip throughput",
        .setup = xdmcp_setup,
        .run = bench_roundtrip,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 2000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b7);

    static const BenchmarkDef b8 = {
        .suite = "xdmcp",
        .name = "packet_tostring",
        .description = "Format XDMCPPacket to string representation",
        .setup = xdmcp_setup,
        .run = bench_tostring,
        .teardown = xdmcp_teardown,
        .default_inner_ops = 2000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b8);
}
