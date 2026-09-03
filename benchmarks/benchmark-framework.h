/*
 * LightDM Performance Benchmarks
 * High-resolution benchmarking framework
 */

#ifndef BENCHMARK_FRAMEWORK_H_
#define BENCHMARK_FRAMEWORK_H_

#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

G_BEGIN_DECLS

typedef void (*BenchSetupFunc) (gpointer *fixture);
typedef void (*BenchRunFunc) (gpointer fixture, guint64 inner_ops);
typedef void (*BenchTeardownFunc) (gpointer fixture);

typedef struct {
    const gchar *suite;
    const gchar *name;
    const gchar *description;
    BenchSetupFunc setup;
    BenchRunFunc run;
    BenchTeardownFunc teardown;
    guint64 default_inner_ops;
    guint default_iterations;
    guint default_warmup;
} BenchmarkDef;

typedef struct {
    guint iterations;
    guint64 inner_ops;
    double min_ns;
    double max_ns;
    double mean_ns;
    double median_ns; /* p50 */
    double p90_ns;
    double p95_ns;
    double p99_ns;
    double stddev_ns;
    double rsd_percent; /* relative standard deviation (%) */
    double ops_per_sec;
    gint64 memory_rss_delta_kb;
} BenchmarkStats;

typedef struct {
    const BenchmarkDef *def;
    BenchmarkStats stats;
} BenchmarkResult;

/* Registration */
void benchmark_register (const BenchmarkDef *def);

/* Execution */
GList *benchmark_run_suite (const gchar *filter_suite,
                           const gchar *filter_name,
                           guint override_iterations,
                           guint override_warmup);

/* Output and Reporting */
void benchmark_print_results_table (GList *results);
gboolean benchmark_export_json (GList *results,
                                const gchar *filepath,
                                guint iterations,
                                guint warmup,
                                GError **error);
gboolean benchmark_compare_json (GList *results,
                                 const gchar *baseline_filepath,
                                 GError **error);

/* Utilities */
guint64 benchmark_get_time_ns (void);
gint64 benchmark_get_process_rss_kb (void);
void benchmark_format_latency (double ns, gchar *buf, gsize buf_size);

/* Suite Initializers */
void bench_config_register (void);
void bench_user_list_register (void);
void bench_xauthority_register (void);
void bench_xdmcp_register (void);

G_END_DECLS

#endif /* BENCHMARK_FRAMEWORK_H_ */
