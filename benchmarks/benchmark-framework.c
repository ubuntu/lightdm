/*
 * LightDM Performance Benchmarks
 * High-resolution benchmarking framework implementation
 */

#include "benchmark-framework.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GRAY    "\033[90m"

static GList *registered_benchmarks = NULL;

void
benchmark_register (const BenchmarkDef *def)
{
    g_return_if_fail (def != NULL);
    registered_benchmarks = g_list_append (registered_benchmarks, (gpointer) def);
}

guint64
benchmark_get_time_ns (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (guint64) ts.tv_sec * 1000000000ULL + (guint64) ts.tv_nsec;
}

gint64
benchmark_get_process_rss_kb (void)
{
    int fd = open ("/proc/self/statm", O_RDONLY);
    if (fd < 0)
        return -1;

    char buffer[256];
    ssize_t bytes_read = read (fd, buffer, sizeof (buffer) - 1);
    close (fd);

    if (bytes_read <= 0)
        return -1;

    buffer[bytes_read] = '\0';

    unsigned long size = 0, resident = 0;
    if (sscanf (buffer, "%lu %lu", &size, &resident) != 2)
        return -1;

    long page_size = sysconf (_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;

    return (gint64) resident * (page_size / 1024);
}

void
benchmark_format_latency (double ns, gchar *buf, gsize buf_size)
{
    if (ns < 1000.0)
        g_snprintf (buf, buf_size, "%6.1f ns", ns);
    else if (ns < 1000000.0)
        g_snprintf (buf, buf_size, "%6.2f us", ns / 1000.0);
    else if (ns < 1000000000.0)
        g_snprintf (buf, buf_size, "%6.2f ms", ns / 1000000.0);
    else
        g_snprintf (buf, buf_size, "%6.2f s ", ns / 1000000000.0);
}

static int
compare_double (const void *a, const void *b)
{
    double da = *(const double *) a;
    double db = *(const double *) b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static void
calculate_stats (double *samples, guint n, guint64 inner_ops, gint64 rss_delta, BenchmarkStats *stats)
{
    qsort (samples, n, sizeof (double), compare_double);

    double sum = 0.0;
    for (guint i = 0; i < n; i++)
        sum += samples[i];

    double mean = sum / (double) n;

    double sum_sq = 0.0;
    for (guint i = 0; i < n; i++)
    {
        double diff = samples[i] - mean;
        sum_sq += diff * diff;
    }

    double stddev = sqrt (sum_sq / (double) (n > 1 ? n - 1 : 1));

    stats->iterations = n;
    stats->inner_ops = inner_ops;
    stats->min_ns = samples[0];
    stats->max_ns = samples[n - 1];
    stats->mean_ns = mean;
    stats->median_ns = (n % 2 == 1) ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) / 2.0;

    guint idx_90 = (guint) ((double) n * 0.90);
    if (idx_90 >= n) idx_90 = n - 1;
    stats->p90_ns = samples[idx_90];

    guint idx_95 = (guint) ((double) n * 0.95);
    if (idx_95 >= n) idx_95 = n - 1;
    stats->p95_ns = samples[idx_95];

    guint idx_99 = (guint) ((double) n * 0.99);
    if (idx_99 >= n) idx_99 = n - 1;
    stats->p99_ns = samples[idx_99];

    stats->stddev_ns = stddev;
    stats->rsd_percent = (mean > 0.0) ? (stddev / mean * 100.0) : 0.0;
    stats->ops_per_sec = (mean > 0.0) ? (1000000000.0 / mean) : 0.0;
    stats->memory_rss_delta_kb = rss_delta;
}

GList *
benchmark_run_suite (const gchar *filter_suite,
                     const gchar *filter_name,
                     guint override_iterations,
                     guint override_warmup)
{
    GList *results = NULL;

    for (GList *l = registered_benchmarks; l; l = l->next)
    {
        const BenchmarkDef *def = (const BenchmarkDef *) l->data;

        if (filter_suite && strlen (filter_suite) > 0)
        {
            if (!g_strrstr (def->suite, filter_suite))
                continue;
        }

        if (filter_name && strlen (filter_name) > 0)
        {
            if (!g_strrstr (def->name, filter_name))
                continue;
        }

        guint iterations = override_iterations ? override_iterations : (def->default_iterations ? def->default_iterations : 30);
        guint warmup = override_warmup ? override_warmup : (def->default_warmup ? def->default_warmup : 5);
        guint64 inner_ops = def->default_inner_ops ? def->default_inner_ops : 1;

        g_print ("%sRunning%s [%s::%s] (%u warmup, %u runs, %lu ops/run)...",
                 ANSI_GRAY, ANSI_RESET, def->suite, def->name, warmup, iterations, (unsigned long) inner_ops);
        fflush (stdout);

        gpointer fixture = NULL;
        if (def->setup)
            def->setup (&fixture);

        /* Warmup phase */
        for (guint i = 0; i < warmup; i++)
            def->run (fixture, inner_ops);

        gint64 rss_before = benchmark_get_process_rss_kb ();

        /* Measurement phase */
        double *samples = g_new0 (double, iterations);
        for (guint i = 0; i < iterations; i++)
        {
            guint64 t0 = benchmark_get_time_ns ();
            def->run (fixture, inner_ops);
            guint64 t1 = benchmark_get_time_ns ();
            samples[i] = (double) (t1 - t0) / (double) inner_ops;
        }

        gint64 rss_after = benchmark_get_process_rss_kb ();
        gint64 rss_delta = (rss_before >= 0 && rss_after >= 0) ? (rss_after - rss_before) : 0;

        if (def->teardown)
            def->teardown (fixture);

        BenchmarkResult *result = g_new0 (BenchmarkResult, 1);
        result->def = def;
        calculate_stats (samples, iterations, inner_ops, rss_delta, &result->stats);
        g_free (samples);

        gchar mean_buf[32];
        benchmark_format_latency (result->stats.mean_ns, mean_buf, sizeof (mean_buf));
        g_print ("\r\033[K%s[OK]%s [%s::%s] %s%s%s (%.1f ops/s, ±%.1f%%)\n",
                 ANSI_GREEN, ANSI_RESET,
                 def->suite, def->name,
                 ANSI_BOLD, mean_buf, ANSI_RESET,
                 result->stats.ops_per_sec, result->stats.rsd_percent);

        results = g_list_append (results, result);
    }

    return results;
}

void
benchmark_print_results_table (GList *results)
{
    if (!results)
    {
        g_print ("No benchmark results to display.\n");
        return;
    }

    g_print ("\n%s========================================================================================================================%s\n", ANSI_BLUE, ANSI_RESET);
    g_print ("%s  LIGHTDM PERFORMANCE BENCHMARK RESULTS%s\n", ANSI_BOLD, ANSI_RESET);
    g_print ("%s========================================================================================================================%s\n", ANSI_BLUE, ANSI_RESET);

    g_print ("%-12s %-28s %11s %11s %11s %11s %11s %7s %14s\n",
             "Suite", "Benchmark", "Mean", "Median(p50)", "Min", "p95", "p99", "RSD%", "Throughput");
    g_print ("------------------------------------------------------------------------------------------------------------------------\n");

    const gchar *current_suite = NULL;

    for (GList *l = results; l; l = l->next)
    {
        BenchmarkResult *r = (BenchmarkResult *) l->data;

        if (!current_suite || strcmp (current_suite, r->def->suite) != 0)
        {
            current_suite = r->def->suite;
        }

        gchar mean_s[32], median_s[32], min_s[32], p95_s[32], p99_s[32];
        benchmark_format_latency (r->stats.mean_ns, mean_s, sizeof (mean_s));
        benchmark_format_latency (r->stats.median_ns, median_s, sizeof (median_s));
        benchmark_format_latency (r->stats.min_ns, min_s, sizeof (min_s));
        benchmark_format_latency (r->stats.p95_ns, p95_s, sizeof (p95_s));
        benchmark_format_latency (r->stats.p99_ns, p99_s, sizeof (p99_s));

        gchar ops_s[32];
        if (r->stats.ops_per_sec >= 1000000.0)
            g_snprintf (ops_s, sizeof (ops_s), "%.2f M ops/s", r->stats.ops_per_sec / 1000000.0);
        else if (r->stats.ops_per_sec >= 1000.0)
            g_snprintf (ops_s, sizeof (ops_s), "%.2f K ops/s", r->stats.ops_per_sec / 1000.0);
        else
            g_snprintf (ops_s, sizeof (ops_s), "%.1f ops/s", r->stats.ops_per_sec);

        const gchar *rsd_color = (r->stats.rsd_percent > 15.0) ? ANSI_YELLOW : ANSI_GREEN;

        g_print ("%-12s %-28s %s%11s%s %11s %11s %11s %11s %s%6.1f%%%s %14s\n",
                 r->def->suite,
                 r->def->name,
                 ANSI_BOLD, mean_s, ANSI_RESET,
                 median_s,
                 min_s,
                 p95_s,
                 p99_s,
                 rsd_color, r->stats.rsd_percent, ANSI_RESET,
                 ops_s);
    }
    g_print ("========================================================================================================================\n\n");
}

gboolean
benchmark_export_json (GList *results,
                       const gchar *filepath,
                       guint iterations,
                       guint warmup,
                       GError **error)
{
    g_return_val_if_fail (filepath != NULL, FALSE);

    GString *json = g_string_new ("{\n");

    /* Metadata */
    g_string_append (json, "  \"metadata\": {\n");
    g_string_append_printf (json, "    \"timestamp\": %ld,\n", (long) time (NULL));
    g_string_append_printf (json, "    \"glib_version\": \"%u.%u.%u\",\n",
                            glib_major_version, glib_minor_version, glib_micro_version);
    g_string_append_printf (json, "    \"num_processors\": %u,\n", g_get_num_processors ());
    g_string_append_printf (json, "    \"iterations\": %u,\n", iterations);
    g_string_append_printf (json, "    \"warmup\": %u\n", warmup);
    g_string_append (json, "  },\n");

    /* Benchmarks list */
    g_string_append (json, "  \"benchmarks\": [\n");

    for (GList *l = results; l; l = l->next)
    {
        BenchmarkResult *r = (BenchmarkResult *) l->data;

        g_string_append (json, "    {\n");
        g_string_append_printf (json, "      \"suite\": \"%s\",\n", r->def->suite);
        g_string_append_printf (json, "      \"name\": \"%s\",\n", r->def->name);
        g_string_append_printf (json, "      \"description\": \"%s\",\n", r->def->description ? r->def->description : "");
        g_string_append_printf (json, "      \"iterations\": %u,\n", r->stats.iterations);
        g_string_append_printf (json, "      \"inner_ops\": %lu,\n", (unsigned long) r->stats.inner_ops);
        g_string_append_printf (json, "      \"mean_ns\": %.2f,\n", r->stats.mean_ns);
        g_string_append_printf (json, "      \"median_ns\": %.2f,\n", r->stats.median_ns);
        g_string_append_printf (json, "      \"min_ns\": %.2f,\n", r->stats.min_ns);
        g_string_append_printf (json, "      \"max_ns\": %.2f,\n", r->stats.max_ns);
        g_string_append_printf (json, "      \"p90_ns\": %.2f,\n", r->stats.p90_ns);
        g_string_append_printf (json, "      \"p95_ns\": %.2f,\n", r->stats.p95_ns);
        g_string_append_printf (json, "      \"p99_ns\": %.2f,\n", r->stats.p99_ns);
        g_string_append_printf (json, "      \"stddev_ns\": %.2f,\n", r->stats.stddev_ns);
        g_string_append_printf (json, "      \"rsd_percent\": %.2f,\n", r->stats.rsd_percent);
        g_string_append_printf (json, "      \"ops_per_sec\": %.2f,\n", r->stats.ops_per_sec);
        g_string_append_printf (json, "      \"rss_delta_kb\": %ld\n", (long) r->stats.memory_rss_delta_kb);

        if (l->next)
            g_string_append (json, "    },\n");
        else
            g_string_append (json, "    }\n");
    }

    g_string_append (json, "  ]\n");
    g_string_append (json, "}\n");

    gboolean ok = g_file_set_contents (filepath, json->str, json->len, error);
    g_string_free (json, TRUE);
    return ok;
}

typedef struct {
    gchar *suite;
    gchar *name;
    double mean_ns;
    double ops_per_sec;
} BaselineItem;

static GHashTable *
load_baseline_items (const gchar *filepath, GError **error)
{
    gchar *content = NULL;
    gsize length = 0;
    if (!g_file_get_contents (filepath, &content, &length, error))
        return NULL;

    GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    /* Lightweight parser for baseline benchmark items */
    gchar **blocks = g_strsplit (content, "{\n", -1);
    for (guint i = 0; blocks[i]; i++)
    {
        gchar *b = blocks[i];
        if (!strstr (b, "\"suite\"") || !strstr (b, "\"name\"") || !strstr (b, "\"mean_ns\""))
            continue;

        gchar *suite = NULL, *name = NULL;
        double mean_ns = 0.0, ops_per_sec = 0.0;

        gchar **lines = g_strsplit (b, "\n", -1);
        for (guint j = 0; lines[j]; j++)
        {
            gchar *line = g_strstrip (lines[j]);
            if (g_str_has_prefix (line, "\"suite\":"))
            {
                gchar **parts = g_strsplit (line, "\"", -1);
                if (g_strv_length (parts) >= 4)
                    suite = g_strdup (parts[3]);
                g_strfreev (parts);
            }
            else if (g_str_has_prefix (line, "\"name\":"))
            {
                gchar **parts = g_strsplit (line, "\"", -1);
                if (g_strv_length (parts) >= 4)
                    name = g_strdup (parts[3]);
                g_strfreev (parts);
            }
            else if (g_str_has_prefix (line, "\"mean_ns\":"))
            {
                const gchar *val = line + strlen ("\"mean_ns\":");
                mean_ns = g_ascii_strtod (val, NULL);
            }
            else if (g_str_has_prefix (line, "\"ops_per_sec\":"))
            {
                const gchar *val = line + strlen ("\"ops_per_sec\":");
                ops_per_sec = g_ascii_strtod (val, NULL);
            }
        }
        g_strfreev (lines);

        if (suite && name && mean_ns > 0.0)
        {
            gchar *key = g_strdup_printf ("%s::%s", suite, name);
            BaselineItem *item = g_new0 (BaselineItem, 1);
            item->suite = suite;
            item->name = name;
            item->mean_ns = mean_ns;
            item->ops_per_sec = ops_per_sec;
            g_hash_table_insert (table, key, item);
        }
        else
        {
            g_free (suite);
            g_free (name);
        }
    }
    g_strfreev (blocks);
    g_free (content);

    return table;
}

gboolean
benchmark_compare_json (GList *results,
                        const gchar *baseline_filepath,
                        GError **error)
{
    GHashTable *baseline = load_baseline_items (baseline_filepath, error);
    if (!baseline)
        return FALSE;

    g_print ("\n%s========================================================================================================================%s\n", ANSI_BLUE, ANSI_RESET);
    g_print ("%s  LIGHTDM PERFORMANCE COMPARISON vs BASELINE: %s%s\n", ANSI_BOLD, baseline_filepath, ANSI_RESET);
    g_print ("%s========================================================================================================================%s\n", ANSI_BLUE, ANSI_RESET);

    g_print ("%-12s %-28s %13s %13s %14s %14s\n",
             "Suite", "Benchmark", "Baseline Mean", "Current Mean", "Diff (Latency)", "Status");
    g_print ("------------------------------------------------------------------------------------------------------------------------\n");

    for (GList *l = results; l; l = l->next)
    {
        BenchmarkResult *r = (BenchmarkResult *) l->data;
        gchar *key = g_strdup_printf ("%s::%s", r->def->suite, r->def->name);
        BaselineItem *base = (BaselineItem *) g_hash_table_lookup (baseline, key);
        g_free (key);

        gchar curr_s[32];
        benchmark_format_latency (r->stats.mean_ns, curr_s, sizeof (curr_s));

        if (base)
        {
            gchar base_s[32];
            benchmark_format_latency (base->mean_ns, base_s, sizeof (base_s));

            double diff_pct = ((r->stats.mean_ns - base->mean_ns) / base->mean_ns) * 100.0;
            const gchar *status_color;
            const gchar *status_text;

            if (diff_pct < -5.0)
            {
                status_color = ANSI_GREEN;
                status_text = "FASTER (OK)";
            }
            else if (diff_pct > 5.0)
            {
                status_color = ANSI_RED;
                status_text = "SLOWER (REGRESSION)";
            }
            else
            {
                status_color = ANSI_GRAY;
                status_text = "SIMILAR (±5%)";
            }

            gchar diff_s[32];
            g_snprintf (diff_s, sizeof (diff_s), "%+.1f%%", diff_pct);

            g_print ("%-12s %-28s %13s %13s %s%14s%s %s%14s%s\n",
                     r->def->suite,
                     r->def->name,
                     base_s,
                     curr_s,
                     status_color, diff_s, ANSI_RESET,
                     status_color, status_text, ANSI_RESET);
        }
        else
        {
            g_print ("%-12s %-28s %13s %13s %14s %14s\n",
                     r->def->suite,
                     r->def->name,
                     "N/A",
                     curr_s,
                     "N/A",
                     "[NEW TEST]");
        }
    }
    g_print ("========================================================================================================================\n\n");

    g_hash_table_destroy (baseline);
    return TRUE;
}
