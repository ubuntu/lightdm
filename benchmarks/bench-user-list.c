/*
 * LightDM Performance Benchmarks
 * Suite: User List & Account Enumeration (bench-user-list.c)
 */

#include "benchmark-framework.h"
#include "user-list.h"
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

typedef struct {
    gchar *username;
    gchar *display_name;
    gchar *home_dir;
    gchar *shell;
    uid_t uid;
    gid_t gid;
} SimulatedUser;

typedef struct {
    gchar *temp_dir;
    SimulatedUser *pool_5000;
    guint pool_size;
    GList *prepopulated_list_1000;
    gchar *sample_dmrc_path;
    gchar **hidden_shells;
    gchar **hidden_users;
} UserListFixture;

static gint
simulated_user_compare (gconstpointer a, gconstpointer b)
{
    const SimulatedUser *u1 = (const SimulatedUser *) a;
    const SimulatedUser *u2 = (const SimulatedUser *) b;
    return g_strcmp0 (u1->display_name, u2->display_name);
}

static void
user_list_setup (gpointer *fixture_out)
{
    UserListFixture *fix = g_new0 (UserListFixture, 1);
    fix->temp_dir = g_dir_make_tmp ("lightdm-bench-user-XXXXXX", NULL);

    fix->pool_size = 5000;
    fix->pool_5000 = g_new0 (SimulatedUser, fix->pool_size);

    /* Generate pseudo-random, shuffled users */
    const gchar *first_names[] = {
        "Alex", "Boris", "Elena", "Dmitry", "Olga", "Sergey", "Anna", "Ivan",
        "Maria", "Pavel", "Natalia", "Mikhail", "Tatiana", "Andrey", "Svetlana"
    };
    const gchar *last_names[] = {
        "Ivanov", "Smirnov", "Kuznetsov", "Popov", "Vasiliev", "Petrov", "Sokolov",
        "Mikhailov", "Novikov", "Fedorov", "Morozov", "Volkov", "Alekseev", "Lebedev"
    };
    guint fn_count = G_N_ELEMENTS (first_names);
    guint ln_count = G_N_ELEMENTS (last_names);

    GRand *rand = g_rand_new_with_seed (42);

    for (guint i = 0; i < fix->pool_size; i++)
    {
        guint fn_idx = g_rand_int_range (rand, 0, fn_count);
        guint ln_idx = g_rand_int_range (rand, 0, ln_count);
        fix->pool_5000[i].username = g_strdup_printf ("user_%05d", (i * 7919) % fix->pool_size);
        fix->pool_5000[i].display_name = g_strdup_printf ("%s %s #%d", first_names[fn_idx], last_names[ln_idx], i);
        fix->pool_5000[i].home_dir = g_strdup_printf ("/home/%s", fix->pool_5000[i].username);
        fix->pool_5000[i].shell = (i % 20 == 0) ? g_strdup ("/bin/false") : g_strdup ("/bin/bash");
        fix->pool_5000[i].uid = 1000 + i;
        fix->pool_5000[i].gid = 1000 + i;
    }
    g_rand_free (rand);

    /* Prepopulate sorted list of 1000 users for lookup benchmarks */
    fix->prepopulated_list_1000 = NULL;
    for (guint i = 0; i < 1000; i++)
    {
        fix->prepopulated_list_1000 = g_list_insert_sorted (
            fix->prepopulated_list_1000,
            &fix->pool_5000[i],
            simulated_user_compare
        );
    }

    /* Sample DMRC file */
    fix->sample_dmrc_path = g_build_filename (fix->temp_dir, "sample.dmrc", NULL);
    const gchar *dmrc_content = "[Desktop]\nSession=xfce\nLanguage=ru_RU.UTF-8\nLayout=us,ru\n";
    g_file_set_contents (fix->sample_dmrc_path, dmrc_content, -1, NULL);

    /* Shell and user filter arrays */
    fix->hidden_shells = g_strsplit ("/bin/false /usr/sbin/nologin /bin/sync /sbin/halt", " ", -1);
    fix->hidden_users = g_strsplit ("nobody nobody4 noaccess daemon sync games", " ", -1);

    *fixture_out = fix;
}

static void
user_list_teardown (gpointer fixture)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    if (!fix) return;

    if (fix->prepopulated_list_1000)
        g_list_free (fix->prepopulated_list_1000);

    for (guint i = 0; i < fix->pool_size; i++)
    {
        g_free (fix->pool_5000[i].username);
        g_free (fix->pool_5000[i].display_name);
        g_free (fix->pool_5000[i].home_dir);
        g_free (fix->pool_5000[i].shell);
    }
    g_free (fix->pool_5000);

    if (fix->sample_dmrc_path)
    {
        g_unlink (fix->sample_dmrc_path);
        g_free (fix->sample_dmrc_path);
    }

    g_strfreev (fix->hidden_shells);
    g_strfreev (fix->hidden_users);

    if (fix->temp_dir)
    {
        g_rmdir (fix->temp_dir);
        g_free (fix->temp_dir);
    }

    g_free (fix);
}

/* Benchmark: Insert & sort 10 users */
static void
bench_insert_sort_10 (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    for (guint64 op = 0; op < inner_ops; op++)
    {
        GList *list = NULL;
        for (guint i = 0; i < 10; i++)
        {
            list = g_list_insert_sorted (list, &fix->pool_5000[i], simulated_user_compare);
        }
        g_list_free (list);
    }
}

/* Benchmark: Insert & sort 100 users */
static void
bench_insert_sort_100 (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    for (guint64 op = 0; op < inner_ops; op++)
    {
        GList *list = NULL;
        for (guint i = 0; i < 100; i++)
        {
            list = g_list_insert_sorted (list, &fix->pool_5000[i], simulated_user_compare);
        }
        g_list_free (list);
    }
}

/* Benchmark: Insert & sort 1000 users (reveals O(N^2) scaling) */
static void
bench_insert_sort_1000 (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    for (guint64 op = 0; op < inner_ops; op++)
    {
        GList *list = NULL;
        for (guint i = 0; i < 1000; i++)
        {
            list = g_list_insert_sorted (list, &fix->pool_5000[i], simulated_user_compare);
        }
        g_list_free (list);
    }
}

/* Benchmark: Insert & sort 5000 users (large LDAP / enterprise simulation) */
static void
bench_insert_sort_5000 (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    for (guint64 op = 0; op < inner_ops; op++)
    {
        GList *list = NULL;
        for (guint i = 0; i < 5000; i++)
        {
            list = g_list_insert_sorted (list, &fix->pool_5000[i], simulated_user_compare);
        }
        g_list_free (list);
    }
}

/* Benchmark: Linear user lookup by username in 1000 users list */
static void
bench_user_lookup_name (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    GList *users = fix->prepopulated_list_1000;

    const gchar *search_targets[] = {
        "user_00000", "user_00500", "user_00999", "user_nonexistent"
    };
    guint num_targets = G_N_ELEMENTS (search_targets);

    volatile gpointer sink = NULL;
    for (guint64 op = 0; op < inner_ops; op++)
    {
        for (guint t = 0; t < num_targets; t++)
        {
            const gchar *target = search_targets[t];
            SimulatedUser *found = NULL;
            for (GList *link = users; link; link = link->next)
            {
                SimulatedUser *u = (SimulatedUser *) link->data;
                if (strcmp (u->username, target) == 0)
                {
                    found = u;
                    break;
                }
            }
            sink = found;
        }
    }
    (void) sink;
}

/* Benchmark: Filter 1000 passwd entries against UID, hidden-shells, hidden-users */
static void
bench_filter_passwd_entries (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;
    uid_t minimum_uid = 1000;
    uid_t maximum_uid = 60000;
    volatile guint sink = 0;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        guint accepted = 0;
        for (guint i = 0; i < 1000; i++)
        {
            SimulatedUser *u = &fix->pool_5000[i];

            /* UID check */
            if (u->uid < minimum_uid || u->uid > maximum_uid)
                continue;

            /* Hidden shell check */
            int s;
            for (s = 0; fix->hidden_shells[s] && strcmp (u->shell, fix->hidden_shells[s]) != 0; s++);
            if (fix->hidden_shells[s])
                continue;

            /* Hidden user check */
            int u_idx;
            for (u_idx = 0; fix->hidden_users[u_idx] && strcmp (u->username, fix->hidden_users[u_idx]) != 0; u_idx++);
            if (fix->hidden_users[u_idx])
                continue;

            accepted++;
        }
        sink += accepted;
    }
    (void) sink;
}

/* Benchmark: Parse DMRC configuration file */
static void
bench_dmrc_parse (gpointer fixture, guint64 inner_ops)
{
    UserListFixture *fix = (UserListFixture *) fixture;

    for (guint64 op = 0; op < inner_ops; op++)
    {
        GKeyFile *dmrc = g_key_file_new ();
        g_key_file_load_from_file (dmrc, fix->sample_dmrc_path, G_KEY_FILE_NONE, NULL);
        gchar *session = g_key_file_get_string (dmrc, "Desktop", "Session", NULL);
        gchar *language = g_key_file_get_string (dmrc, "Desktop", "Language", NULL);
        g_free (session);
        g_free (language);
        g_key_file_free (dmrc);
    }
}

void
bench_user_list_register (void)
{
    static const BenchmarkDef b1 = {
        .suite = "user_list",
        .name = "sort_insert_10",
        .description = "Sort-insert 10 users into UserList",
        .setup = user_list_setup,
        .run = bench_insert_sort_10,
        .teardown = user_list_teardown,
        .default_inner_ops = 500,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b1);

    static const BenchmarkDef b2 = {
        .suite = "user_list",
        .name = "sort_insert_100",
        .description = "Sort-insert 100 users into UserList",
        .setup = user_list_setup,
        .run = bench_insert_sort_100,
        .teardown = user_list_teardown,
        .default_inner_ops = 50,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b2);

    static const BenchmarkDef b3 = {
        .suite = "user_list",
        .name = "sort_insert_1000",
        .description = "Sort-insert 1000 users (O(N^2) scaling test)",
        .setup = user_list_setup,
        .run = bench_insert_sort_1000,
        .teardown = user_list_teardown,
        .default_inner_ops = 5,
        .default_iterations = 25,
        .default_warmup = 3
    };
    benchmark_register (&b3);

    static const BenchmarkDef b4 = {
        .suite = "user_list",
        .name = "sort_insert_5000",
        .description = "Sort-insert 5000 users (large directory / LDAP scale)",
        .setup = user_list_setup,
        .run = bench_insert_sort_5000,
        .teardown = user_list_teardown,
        .default_inner_ops = 1,
        .default_iterations = 10,
        .default_warmup = 2
    };
    benchmark_register (&b4);

    static const BenchmarkDef b5 = {
        .suite = "user_list",
        .name = "lookup_by_name",
        .description = "User lookup by name in 1000-user list",
        .setup = user_list_setup,
        .run = bench_user_lookup_name,
        .teardown = user_list_teardown,
        .default_inner_ops = 500,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b5);

    static const BenchmarkDef b6 = {
        .suite = "user_list",
        .name = "filter_passwd_entries",
        .description = "UID and hidden-shells/users filtering on 1000 entries",
        .setup = user_list_setup,
        .run = bench_filter_passwd_entries,
        .teardown = user_list_teardown,
        .default_inner_ops = 100,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b6);

    static const BenchmarkDef b7 = {
        .suite = "user_list",
        .name = "dmrc_parse",
        .description = "Parse ~/.dmrc session and language file",
        .setup = user_list_setup,
        .run = bench_dmrc_parse,
        .teardown = user_list_teardown,
        .default_inner_ops = 200,
        .default_iterations = 30,
        .default_warmup = 5
    };
    benchmark_register (&b7);
}
