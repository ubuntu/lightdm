/*
 * LightDM Performance Benchmarks
 * Main Benchmark Runner CLI (lightdm-benchmark.c)
 */

#include "benchmark-framework.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>

static gchar *filter_suite = NULL;
static gchar *filter_name = NULL;
static gchar *filter_general = NULL;
static gint opt_iterations = 0;
static gint opt_warmup = 0;
static gchar *opt_output_json = NULL;
static gchar *opt_compare_json = NULL;
static gboolean opt_json_stdout = FALSE;
static gboolean opt_list_only = FALSE;

static const GOptionEntry entries[] = {
    { "filter", 'f', 0, G_OPTION_ARG_STRING, &filter_general, "Filter benchmarks by suite or name substring", "PATTERN" },
    { "suite", 's', 0, G_OPTION_ARG_STRING, &filter_suite, "Filter by suite name (config, user_list, xauthority, xdmcp)", "SUITE" },
    { "name", 'n', 0, G_OPTION_ARG_STRING, &filter_name, "Filter by benchmark name substring", "NAME" },
    { "iterations", 'i', 0, G_OPTION_ARG_INT, &opt_iterations, "Number of measurement iterations (default: suite-specific, ~30)", "N" },
    { "warmup", 'w', 0, G_OPTION_ARG_INT, &opt_warmup, "Number of warmup iterations (default: suite-specific, ~5)", "N" },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_output_json, "Export benchmark results to JSON file", "FILE" },
    { "compare", 'c', 0, G_OPTION_ARG_FILENAME, &opt_compare_json, "Compare results against a baseline JSON file", "BASELINE" },
    { "json", 'j', 0, G_OPTION_ARG_NONE, &opt_json_stdout, "Output results to stdout in JSON format", NULL },
    { "list", 'l', 0, G_OPTION_ARG_NONE, &opt_list_only, "List all available benchmarks and exit", NULL },
    { NULL }
};

int
main (int argc, char **argv)
{
    g_setenv ("G_MESSAGES_PREFIXED", "0", TRUE);

    GError *error = NULL;
    GOptionContext *context = g_option_context_new ("- LightDM Performance Benchmark Suite");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("Option parsing failed: %s\n", error->message);
        g_error_free (error);
        g_option_context_free (context);
        return EXIT_FAILURE;
    }
    g_option_context_free (context);

    /* Register all benchmark suites */
    bench_config_register ();
    bench_user_list_register ();
    bench_xauthority_register ();
    bench_xdmcp_register ();

    if (opt_list_only)
    {
        g_print ("Available benchmarks:\n");
        GList *results = benchmark_run_suite ("__LIST_NOTHING__", NULL, 0, 0);
        (void) results;
        return EXIT_SUCCESS;
    }

    const gchar *s_filter = filter_suite;
    const gchar *n_filter = filter_name;

    if (filter_general && strlen (filter_general) > 0)
    {
        /* If general filter matches suite, use as suite; else name */
        if (strcmp (filter_general, "config") == 0 ||
            strcmp (filter_general, "user_list") == 0 ||
            strcmp (filter_general, "xauthority") == 0 ||
            strcmp (filter_general, "xdmcp") == 0)
        {
            s_filter = filter_general;
        }
        else
        {
            n_filter = filter_general;
        }
    }

    if (!opt_json_stdout)
    {
        g_print ("\nStarting LightDM Benchmarks (GLib %u.%u.%u, %u CPUs)...\n\n",
                 glib_major_version, glib_minor_version, glib_micro_version,
                 g_get_num_processors ());
    }

    GList *results = benchmark_run_suite (
        s_filter,
        n_filter,
        (opt_iterations > 0) ? (guint) opt_iterations : 0,
        (opt_warmup > 0) ? (guint) opt_warmup : 0
    );

    if (!results)
    {
        g_printerr ("No benchmarks matched the specified filters.\n");
        return EXIT_FAILURE;
    }

    /* Print formatted table */
    if (!opt_json_stdout)
    {
        benchmark_print_results_table (results);
    }

    /* Compare against baseline if requested */
    if (opt_compare_json)
    {
        GError *comp_err = NULL;
        if (!benchmark_compare_json (results, opt_compare_json, &comp_err))
        {
            g_printerr ("Failed to compare with baseline %s: %s\n", opt_compare_json, comp_err->message);
            g_clear_error (&comp_err);
        }
    }

    /* Save to output JSON if requested */
    if (opt_output_json)
    {
        GError *exp_err = NULL;
        if (!benchmark_export_json (results, opt_output_json, opt_iterations, opt_warmup, &exp_err))
        {
            g_printerr ("Failed to export results to %s: %s\n", opt_output_json, exp_err->message);
            g_clear_error (&exp_err);
        }
        else if (!opt_json_stdout)
        {
            g_print ("Results saved to %s\n", opt_output_json);
        }
    }

    /* Print JSON to stdout if requested */
    if (opt_json_stdout)
    {
        gchar *tmp_json = g_build_filename (g_get_tmp_dir (), "lightdm_bench_stdout.json", NULL);
        if (benchmark_export_json (results, tmp_json, opt_iterations, opt_warmup, NULL))
        {
            gchar *content = NULL;
            if (g_file_get_contents (tmp_json, &content, NULL, NULL))
            {
                g_print ("%s", content);
                g_free (content);
            }
            g_unlink (tmp_json);
        }
        g_free (tmp_json);
    }

    /* Cleanup results */
    for (GList *l = results; l; l = l->next)
        g_free (l->data);
    g_list_free (results);

    g_free (filter_suite);
    g_free (filter_name);
    g_free (filter_general);
    g_free (opt_output_json);
    g_free (opt_compare_json);

    return EXIT_SUCCESS;
}
