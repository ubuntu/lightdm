/*
 * LightDM Performance Benchmarks
 * Suite: Configuration Engine (bench-config.c)
 */

#include "benchmark-framework.h"
#include "configuration.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>

typedef struct {
    gchar *temp_dir;
    gchar *small_conf;
    gchar *standard_conf;
    gchar *large_conf;
    gchar **dropin_confs_5;
    gchar **dropin_confs_20;
    Configuration *preloaded_config;
} ConfigFixture;

static gchar *
generate_conf_content (gint num_seats, gint keys_per_seat)
{
    GString *s = g_string_new (NULL);
    g_string_append (s, "[LightDM]\n");
    g_string_append (s, "start-default-seat=true\n");
    g_string_append (s, "greeter-user=lightdm\n");
    g_string_append (s, "minimum-vt=7\n");
    g_string_append (s, "user-authority-in-system-dir=false\n");
    g_string_append (s, "guest-account-script=guest-account\n");
    g_string_append (s, "logind-check-graphical=true\n\n");

    g_string_append (s, "[Seat:*]\n");
    g_string_append (s, "greeter-session=lightdm-gtk-greeter\n");
    g_string_append (s, "user-session=default\n");
    g_string_append (s, "session-wrapper=lightdm-session\n");
    g_string_append (s, "autologin-user=devuan\n");
    g_string_append (s, "autologin-user-timeout=0\n");
    g_string_append (s, "greeter-hide-users=false\n");
    g_string_append (s, "greeter-allow-guest=true\n\n");

    for (int i = 0; i < num_seats; i++)
    {
        g_string_append_printf (s, "[Seat:seat%d]\n", i);
        g_string_append_printf (s, "greeter-session=greeter-%d\n", i);
        g_string_append_printf (s, "user-session=session-%d\n", i);
        for (int k = 0; k < keys_per_seat; k++)
        {
            g_string_append_printf (s, "xserver-command=X -core -noreset -seat seat%d -auth /tmp/auth%d_%d\n", i, i, k);
        }
        g_string_append (s, "\n");
    }

    g_string_append (s, "[XDMCPServer]\n");
    g_string_append (s, "enabled=false\n");
    g_string_append (s, "port=177\n");
    g_string_append (s, "hostname=localhost\n\n");

    g_string_append (s, "[VNCServer]\n");
    g_string_append (s, "enabled=false\n");
    g_string_append (s, "port=5900\n");
    g_string_append (s, "width=1024\n");
    g_string_append (s, "height=768\n");
    g_string_append (s, "depth=24\n");

    return g_string_free (s, FALSE);
}

static void
config_setup (gpointer *fixture_out)
{
    ConfigFixture *fix = g_new0 (ConfigFixture, 1);
    fix->temp_dir = g_dir_make_tmp ("lightdm-bench-config-XXXXXX", NULL);

    /* Small config */
    fix->small_conf = g_build_filename (fix->temp_dir, "small.conf", NULL);
    gchar *small_content = "[LightDM]\nstart-default-seat=true\n\n[Seat:*]\ngreeter-session=test\n";
    g_file_set_contents (fix->small_conf, small_content, -1, NULL);

    /* Standard config (~150 lines) */
    fix->standard_conf = g_build_filename (fix->temp_dir, "standard.conf", NULL);
    gchar *standard_content = generate_conf_content (5, 4);
    g_file_set_contents (fix->standard_conf, standard_content, -1, NULL);
    g_free (standard_content);

    /* Large config (~1000 lines, 50 seats) */
    fix->large_conf = g_build_filename (fix->temp_dir, "large.conf", NULL);
    gchar *large_content = generate_conf_content (50, 4);
    g_file_set_contents (fix->large_conf, large_content, -1, NULL);
    g_free (large_content);

    /* 5 drop-in configs */
    fix->dropin_confs_5 = g_new0 (gchar *, 6);
    for (int i = 0; i < 5; i++)
    {
        gchar *filename = g_strdup_printf ("dropin5_%02d.conf", i);
        fix->dropin_confs_5[i] = g_build_filename (fix->temp_dir, filename, NULL);
        g_free (filename);
        gchar *content = g_strdup_printf ("[Seat:*]\ngreeter-session=greeter-override-%d\n[Seat:seat%d]\nuser-session=override-%d\n", i, i, i);
        g_file_set_contents (fix->dropin_confs_5[i], content, -1, NULL);
        g_free (content);
    }

    /* 20 drop-in configs */
    fix->dropin_confs_20 = g_new0 (gchar *, 21);
    for (int i = 0; i < 20; i++)
    {
        gchar *filename = g_strdup_printf ("dropin20_%02d.conf", i);
        fix->dropin_confs_20[i] = g_build_filename (fix->temp_dir, filename, NULL);
        g_free (filename);
        gchar *content = g_strdup_printf ("[Seat:*]\ngreeter-wrapper=wrapper-%d\n[Seat:seat%d]\nxserver-command=X :%d\n", i, i, i);
        g_file_set_contents (fix->dropin_confs_20[i], content, -1, NULL);
        g_free (content);
    }

    /* Preloaded configuration for key lookup and seat globbing */
    fix->preloaded_config = g_object_new (CONFIGURATION_TYPE, NULL);
    config_load_from_file (fix->preloaded_config, fix->large_conf, NULL, NULL);

    *fixture_out = fix;
}

static void
config_teardown (gpointer fixture)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    if (!fix) return;

    if (fix->preloaded_config)
        g_object_unref (fix->preloaded_config);

    if (fix->small_conf)
    {
        g_unlink (fix->small_conf);
        g_free (fix->small_conf);
    }
    if (fix->standard_conf)
    {
        g_unlink (fix->standard_conf);
        g_free (fix->standard_conf);
    }
    if (fix->large_conf)
    {
        g_unlink (fix->large_conf);
        g_free (fix->large_conf);
    }

    if (fix->dropin_confs_5)
    {
        for (int i = 0; fix->dropin_confs_5[i]; i++)
        {
            g_unlink (fix->dropin_confs_5[i]);
            g_free (fix->dropin_confs_5[i]);
        }
        g_free (fix->dropin_confs_5);
    }

    if (fix->dropin_confs_20)
    {
        for (int i = 0; fix->dropin_confs_20[i]; i++)
        {
            g_unlink (fix->dropin_confs_20[i]);
            g_free (fix->dropin_confs_20[i]);
        }
        g_free (fix->dropin_confs_20);
    }

    if (fix->temp_dir)
    {
        g_rmdir (fix->temp_dir);
        g_free (fix->temp_dir);
    }

    g_free (fix);
}

/* Benchmark: Load small config */
static void
bench_load_small (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    for (guint64 i = 0; i < inner_ops; i++)
    {
        Configuration *c = g_object_new (CONFIGURATION_TYPE, NULL);
        config_load_from_file (c, fix->small_conf, NULL, NULL);
        g_object_unref (c);
    }
}

/* Benchmark: Load standard config */
static void
bench_load_standard (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    for (guint64 i = 0; i < inner_ops; i++)
    {
        Configuration *c = g_object_new (CONFIGURATION_TYPE, NULL);
        config_load_from_file (c, fix->standard_conf, NULL, NULL);
        g_object_unref (c);
    }
}

/* Benchmark: Load large config (50 seats) */
static void
bench_load_large (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    for (guint64 i = 0; i < inner_ops; i++)
    {
        Configuration *c = g_object_new (CONFIGURATION_TYPE, NULL);
        config_load_from_file (c, fix->large_conf, NULL, NULL);
        g_object_unref (c);
    }
}

/* Benchmark: Merge base config + 5 drop-ins */
static void
bench_merge_5 (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    for (guint64 i = 0; i < inner_ops; i++)
    {
        Configuration *c = g_object_new (CONFIGURATION_TYPE, NULL);
        config_load_from_file (c, fix->standard_conf, NULL, NULL);
        for (int j = 0; j < 5; j++)
            config_load_from_file (c, fix->dropin_confs_5[j], NULL, NULL);
        g_object_unref (c);
    }
}

/* Benchmark: Merge base config + 20 drop-ins */
static void
bench_merge_20 (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    for (guint64 i = 0; i < inner_ops; i++)
    {
        Configuration *c = g_object_new (CONFIGURATION_TYPE, NULL);
        config_load_from_file (c, fix->standard_conf, NULL, NULL);
        for (int j = 0; j < 20; j++)
            config_load_from_file (c, fix->dropin_confs_20[j], NULL, NULL);
        g_object_unref (c);
    }
}

/* Benchmark: Seat pattern globbing resolution (as in lightdm.c) */
static void
bench_seat_globbing (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    Configuration *c = fix->preloaded_config;

    const gchar *test_seat_names[] = { "seat0", "seat1", "seat15", "seat42", "seat-unknown", NULL };

    for (guint64 i = 0; i < inner_ops; i++)
    {
        for (int s = 0; test_seat_names[s]; s++)
        {
            const gchar *seat_name = test_seat_names[s];
            GList *sections = g_list_append (NULL, g_strdup ("Seat:*"));
            g_auto(GStrv) groups = config_get_groups (c);
            const size_t seat_len = strlen ("Seat:");
            for (gchar **g = groups; *g; g++)
            {
                if (g_str_has_prefix (*g, "Seat:") && strcmp (*g, "Seat:*") != 0)
                {
                    const gchar *seat_glob = *g + seat_len;
                    if (g_pattern_match_simple (seat_glob, seat_name))
                        sections = g_list_append (sections, g_strdup (*g));
                }
            }
            g_list_free_full (sections, g_free);
        }
    }
}

/* Benchmark: Key lookup string */
static void
bench_lookup_string (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    Configuration *c = fix->preloaded_config;

    for (guint64 i = 0; i < inner_ops; i++)
    {
        gchar *val1 = config_get_string (c, "LightDM", "greeter-user");
        gchar *val2 = config_get_string (c, "Seat:*", "greeter-session");
        gchar *val3 = config_get_string (c, "Seat:seat10", "greeter-session");
        gchar *val4 = config_get_string (c, "Seat:seat25", "xserver-command");
        g_free (val1);
        g_free (val2);
        g_free (val3);
        g_free (val4);
    }
}

/* Benchmark: Key lookup boolean & integer */
static void
bench_lookup_bool_int (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    Configuration *c = fix->preloaded_config;

    volatile gboolean b_res;
    volatile gint i_res;

    for (guint64 i = 0; i < inner_ops; i++)
    {
        b_res = config_get_boolean (c, "LightDM", "start-default-seat");
        b_res = config_get_boolean (c, "Seat:*", "greeter-hide-users");
        b_res = config_get_boolean (c, "XDMCPServer", "enabled");
        i_res = config_get_integer (c, "LightDM", "minimum-vt");
        i_res = config_get_integer (c, "VNCServer", "width");
        i_res = config_get_integer (c, "VNCServer", "height");
    }
    (void) b_res;
    (void) i_res;
}

/* Benchmark: config_has_key check */
static void
bench_has_key (gpointer fixture, guint64 inner_ops)
{
    ConfigFixture *fix = (ConfigFixture *) fixture;
    Configuration *c = fix->preloaded_config;

    volatile gboolean exists;

    for (guint64 i = 0; i < inner_ops; i++)
    {
        exists = config_has_key (c, "LightDM", "start-default-seat");
        exists = config_has_key (c, "Seat:*", "greeter-session");
        exists = config_has_key (c, "Seat:seat5", "user-session");
        exists = config_has_key (c, "Seat:seat5", "non-existent-key");
        exists = config_has_key (c, "NonExistentSection", "key");
    }
    (void) exists;
}

void
bench_config_register (void)
{
    static const BenchmarkDef b1 = {
        .suite = "config",
        .name = "load_small",
        .description = "Parse small configuration file (~10 lines)",
        .setup = config_setup,
        .run = bench_load_small,
        .teardown = config_teardown,
        .default_inner_ops = 50,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b1);

    static const BenchmarkDef b2 = {
        .suite = "config",
        .name = "load_standard",
        .description = "Parse standard configuration file (~150 lines)",
        .setup = config_setup,
        .run = bench_load_standard,
        .teardown = config_teardown,
        .default_inner_ops = 20,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b2);

    static const BenchmarkDef b3 = {
        .suite = "config",
        .name = "load_large_50seats",
        .description = "Parse large configuration file (~1000 lines, 50 seats)",
        .setup = config_setup,
        .run = bench_load_large,
        .teardown = config_teardown,
        .default_inner_ops = 5,
        .default_iterations = 25,
        .default_warmup = 3
    };
    benchmark_register (&b3);

    static const BenchmarkDef b4 = {
        .suite = "config",
        .name = "merge_5_files",
        .description = "Load base config and merge 5 drop-in config files",
        .setup = config_setup,
        .run = bench_merge_5,
        .teardown = config_teardown,
        .default_inner_ops = 10,
        .default_iterations = 25,
        .default_warmup = 3
    };
    benchmark_register (&b4);

    static const BenchmarkDef b5 = {
        .suite = "config",
        .name = "merge_20_files",
        .description = "Load base config and merge 20 drop-in config files",
        .setup = config_setup,
        .run = bench_merge_20,
        .teardown = config_teardown,
        .default_inner_ops = 5,
        .default_iterations = 20,
        .default_warmup = 3
    };
    benchmark_register (&b5);

    static const BenchmarkDef b6 = {
        .suite = "config",
        .name = "seat_glob_resolve",
        .description = "Seat pattern globbing across 50 seat sections",
        .setup = config_setup,
        .run = bench_seat_globbing,
        .teardown = config_teardown,
        .default_inner_ops = 100,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b6);

    static const BenchmarkDef b7 = {
        .suite = "config",
        .name = "key_lookup_string",
        .description = "Throughput of config_get_string() lookups",
        .setup = config_setup,
        .run = bench_lookup_string,
        .teardown = config_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b7);

    static const BenchmarkDef b8 = {
        .suite = "config",
        .name = "key_lookup_bool_int",
        .description = "Throughput of get_boolean() and get_integer() lookups",
        .setup = config_setup,
        .run = bench_lookup_bool_int,
        .teardown = config_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b8);

    static const BenchmarkDef b9 = {
        .suite = "config",
        .name = "key_has_key",
        .description = "Throughput of config_has_key() presence checks",
        .setup = config_setup,
        .run = bench_has_key,
        .teardown = config_teardown,
        .default_inner_ops = 5000,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b9);
}
