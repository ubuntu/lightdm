/*
 * LightDM Performance Benchmarks
 * Suite: Xauthority Operations (bench-xauthority.c)
 */

#include "benchmark-framework.h"
#include "x-authority.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    gchar *temp_dir;
    gchar *single_record_file;
    gchar *multi_record_file_10;
    gchar *multi_record_file_50;
    XAuthority *test_cookie;
} XAuthFixture;

static void
xauth_setup (gpointer *fixture_out)
{
    XAuthFixture *fix = g_new0 (XAuthFixture, 1);
    fix->temp_dir = g_dir_make_tmp ("lightdm-bench-xauth-XXXXXX", NULL);

    const guint8 addr[] = { 127, 0, 0, 1 };
    fix->test_cookie = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), "0");

    /* Single record file */
    fix->single_record_file = g_build_filename (fix->temp_dir, "xauth_1", NULL);
    x_authority_write (fix->test_cookie, XAUTH_WRITE_MODE_REPLACE, fix->single_record_file, NULL);

    /* 10 records file */
    fix->multi_record_file_10 = g_build_filename (fix->temp_dir, "xauth_10", NULL);
    for (int i = 0; i < 10; i++)
    {
        gchar num[16];
        g_snprintf (num, sizeof (num), "%d", i);
        XAuthority *a = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), num);
        x_authority_write (a, XAUTH_WRITE_MODE_REPLACE, fix->multi_record_file_10, NULL);
        g_object_unref (a);
    }

    /* 50 records file */
    fix->multi_record_file_50 = g_build_filename (fix->temp_dir, "xauth_50", NULL);
    for (int i = 0; i < 50; i++)
    {
        gchar num[16];
        g_snprintf (num, sizeof (num), "%d", i);
        XAuthority *a = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), num);
        x_authority_write (a, XAUTH_WRITE_MODE_REPLACE, fix->multi_record_file_50, NULL);
        g_object_unref (a);
    }

    *fixture_out = fix;
}

static void
xauth_teardown (gpointer fixture)
{
    XAuthFixture *fix = (XAuthFixture *) fixture;
    if (!fix) return;

    if (fix->test_cookie)
        g_object_unref (fix->test_cookie);

    if (fix->single_record_file)
    {
        g_unlink (fix->single_record_file);
        g_free (fix->single_record_file);
    }
    if (fix->multi_record_file_10)
    {
        g_unlink (fix->multi_record_file_10);
        g_free (fix->multi_record_file_10);
    }
    if (fix->multi_record_file_50)
    {
        g_unlink (fix->multi_record_file_50);
        g_free (fix->multi_record_file_50);
    }

    if (fix->temp_dir)
    {
        g_rmdir (fix->temp_dir);
        g_free (fix->temp_dir);
    }

    g_free (fix);
}

/* Benchmark: Generating MIT-MAGIC-COOKIE-1 auth cookies */
static void
bench_generate_cookie (gpointer fixture, guint64 inner_ops)
{
    (void) fixture;
    const guint8 addr[] = { 192, 168, 1, 100 };

    for (guint64 op = 0; op < inner_ops; op++)
    {
        XAuthority *auth = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), ":0");
        g_object_unref (auth);
    }
}

/* Benchmark: Parse binary Xauthority file with 10 records */
static void
bench_parse_file_10 (gpointer fixture, guint64 inner_ops)
{
    XAuthFixture *fix = (XAuthFixture *) fixture;
    const guint8 addr[] = { 127, 0, 0, 1 };

    for (guint64 op = 0; op < inner_ops; op++)
    {
        /* x_authority_write with XAUTH_WRITE_MODE_REPLACE reads existing records,
           matches, and writes back. Here we check reading/matching speed. */
        XAuthority *query = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), "9");
        x_authority_write (query, XAUTH_WRITE_MODE_REPLACE, fix->multi_record_file_10, NULL);
        g_object_unref (query);
    }
}

/* Benchmark: Parse binary Xauthority file with 50 records */
static void
bench_parse_file_50 (gpointer fixture, guint64 inner_ops)
{
    XAuthFixture *fix = (XAuthFixture *) fixture;
    const guint8 addr[] = { 127, 0, 0, 1 };

    for (guint64 op = 0; op < inner_ops; op++)
    {
        XAuthority *query = x_authority_new_cookie (XAUTH_FAMILY_LOCAL, addr, sizeof (addr), "49");
        x_authority_write (query, XAUTH_WRITE_MODE_REPLACE, fix->multi_record_file_50, NULL);
        g_object_unref (query);
    }
}

/* Benchmark: Write new record to a new file */
static void
bench_write_new (gpointer fixture, guint64 inner_ops)
{
    XAuthFixture *fix = (XAuthFixture *) fixture;
    gchar *tmp_file = g_build_filename (fix->temp_dir, "bench_write_tmp", NULL);

    for (guint64 op = 0; op < inner_ops; op++)
    {
        x_authority_write (fix->test_cookie, XAUTH_WRITE_MODE_SET, tmp_file, NULL);
    }

    g_unlink (tmp_file);
    g_free (tmp_file);
}

void
bench_xauthority_register (void)
{
    static const BenchmarkDef b1 = {
        .suite = "xauthority",
        .name = "generate_cookie",
        .description = "Generate MIT-MAGIC-COOKIE-1 authentication cookie",
        .setup = xauth_setup,
        .run = bench_generate_cookie,
        .teardown = xauth_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b1);

    static const BenchmarkDef b2 = {
        .suite = "xauthority",
        .name = "write_single_record",
        .description = "Atomic write of single Xauthority record to disk",
        .setup = xauth_setup,
        .run = bench_write_new,
        .teardown = xauth_teardown,
        .default_inner_ops = 100,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b2);

    static const BenchmarkDef b3 = {
        .suite = "xauthority",
        .name = "update_record_10_file",
        .description = "Parse, match and update record in 10-entry Xauthority file",
        .setup = xauth_setup,
        .run = bench_parse_file_10,
        .teardown = xauth_teardown,
        .default_inner_ops = 50,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b3);

    static const BenchmarkDef b4 = {
        .suite = "xauthority",
        .name = "update_record_50_file",
        .description = "Parse, match and update record in 50-entry Xauthority file",
        .setup = xauth_setup,
        .run = bench_parse_file_50,
        .teardown = xauth_teardown,
        .default_inner_ops = 20,
        .default_iterations = 25,
        .default_warmup = 3
    };
    benchmark_register (&b4);
}
