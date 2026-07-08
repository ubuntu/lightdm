/*
 * Copyright (C) 2026 The LightDM Authors.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "../../src/x-authority.h"

typedef struct
{
    guint16 family;
    const guint8 *address;
    guint16 address_length;
    const gchar *number;
    const gchar *authorization_name;
    const guint8 *authorization_data;
    guint16 authorization_data_length;
} AuthorityRecord;

static const guint8 target_address[] = "lightdm-test";
static const guint8 unrelated_address[] = "unrelated-host";

static const guint8 stale_cookie_1[16] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F
};
static const guint8 stale_cookie_2[16] = {
    0x10, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x1F
};
static const guint8 new_cookie[16] = {
    0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F
};
static const guint8 other_authorization_data[8] = {
    0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37
};
static const guint8 unrelated_cookie[16] = {
    0xF0, 0xF1, 0xF2, 0xF3,
    0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB,
    0xFC, 0xFD, 0xFE, 0xFF
};

static const AuthorityRecord matching_record_1 = {
    XAUTH_FAMILY_LOCAL,
    target_address,
    sizeof (target_address) - 1,
    "0",
    "MIT-MAGIC-COOKIE-1",
    stale_cookie_1,
    sizeof (stale_cookie_1)
};
static const AuthorityRecord matching_record_2 = {
    XAUTH_FAMILY_LOCAL,
    target_address,
    sizeof (target_address) - 1,
    "0",
    "MIT-MAGIC-COOKIE-1",
    stale_cookie_2,
    sizeof (stale_cookie_2)
};
static const AuthorityRecord other_authorization_record = {
    XAUTH_FAMILY_LOCAL,
    target_address,
    sizeof (target_address) - 1,
    "0",
    "XDM-AUTHORIZATION-1",
    other_authorization_data,
    sizeof (other_authorization_data)
};
static const AuthorityRecord unrelated_record = {
    XAUTH_FAMILY_LOCAL,
    unrelated_address,
    sizeof (unrelated_address) - 1,
    "9",
    "MIT-MAGIC-COOKIE-1",
    unrelated_cookie,
    sizeof (unrelated_cookie)
};
static const AuthorityRecord new_record = {
    XAUTH_FAMILY_LOCAL,
    target_address,
    sizeof (target_address) - 1,
    "0",
    "MIT-MAGIC-COOKIE-1",
    new_cookie,
    sizeof (new_cookie)
};

static void
append_card16 (GByteArray *data, guint16 value)
{
    const guint8 bytes[2] = { value >> 8, value & 0xFF };
    g_byte_array_append (data, bytes, sizeof (bytes));
}

static void
append_data (GByteArray *data, const guint8 *value, guint16 value_length)
{
    append_card16 (data, value_length);
    g_byte_array_append (data, value, value_length);
}

static void
append_string (GByteArray *data, const gchar *value)
{
    append_data (data, (const guint8 *) value, strlen (value));
}

static void
append_record (GByteArray *data, const AuthorityRecord *record)
{
    append_card16 (data, record->family);
    append_data (data, record->address, record->address_length);
    append_string (data, record->number);
    append_string (data, record->authorization_name);
    append_data (data, record->authorization_data, record->authorization_data_length);
}

static GByteArray *
serialize_records (const AuthorityRecord *const *records, gsize records_length)
{
    GByteArray *data = g_byte_array_new ();
    for (gsize i = 0; i < records_length; i++)
        append_record (data, records[i]);

    return data;
}

static void
write_records (const gchar *filename, const AuthorityRecord *const *records, gsize records_length)
{
    g_autoptr(GByteArray) data = serialize_records (records, records_length);
    g_autoptr(GError) error = NULL;
    g_assert_true (g_file_set_contents (filename, (const gchar *) data->data, data->len, &error));
    g_assert_no_error (error);
}

static void
assert_records (const gchar *filename, const AuthorityRecord *const *records, gsize records_length)
{
    g_autoptr(GByteArray) expected = serialize_records (records, records_length);
    g_autofree gchar *actual = NULL;
    gsize actual_length = 0;
    g_autoptr(GError) error = NULL;
    g_assert_true (g_file_get_contents (filename, &actual, &actual_length, &error));
    g_assert_no_error (error);
    g_assert_cmpuint (actual_length, ==, expected->len);
    g_assert_cmpint (memcmp (actual, expected->data, actual_length), ==, 0);
}

static gchar *
create_authority_file (const AuthorityRecord *const *records, gsize records_length)
{
    g_autoptr(GError) error = NULL;
    gchar *filename = NULL;
    gint fd = g_file_open_tmp ("lightdm-xauthority-XXXXXX", &filename, &error);
    g_assert_no_error (error);
    g_assert_cmpint (fd, >=, 0);
    close (fd);

    write_records (filename, records, records_length);
    return filename;
}

static XAuthority *
create_new_authority (void)
{
    return x_authority_new (new_record.family,
                            new_record.address,
                            new_record.address_length,
                            new_record.number,
                            new_record.authorization_name,
                            new_record.authorization_data,
                            new_record.authorization_data_length);
}

static void
test_replace_deduplicates (void)
{
    const AuthorityRecord *input[] = {
        &matching_record_1,
        &matching_record_2,
        &other_authorization_record,
        &unrelated_record
    };
    const AuthorityRecord *expected[] = {
        &new_record,
        &other_authorization_record,
        &unrelated_record
    };
    g_autofree gchar *filename = create_authority_file (input, G_N_ELEMENTS (input));
    g_autoptr(XAuthority) authority = create_new_authority ();
    g_autoptr(GError) error = NULL;

    g_assert_true (x_authority_write (authority, XAUTH_WRITE_MODE_REPLACE, filename, &error));
    g_assert_no_error (error);
    assert_records (filename, expected, G_N_ELEMENTS (expected));
    g_unlink (filename);
}

static void
test_replace_missing_appends (void)
{
    const AuthorityRecord *input[] = {
        &other_authorization_record,
        &unrelated_record
    };
    const AuthorityRecord *expected[] = {
        &other_authorization_record,
        &unrelated_record,
        &new_record
    };
    g_autofree gchar *filename = create_authority_file (input, G_N_ELEMENTS (input));
    g_autoptr(XAuthority) authority = create_new_authority ();
    g_autoptr(GError) error = NULL;

    g_assert_true (x_authority_write (authority, XAUTH_WRITE_MODE_REPLACE, filename, &error));
    g_assert_no_error (error);
    assert_records (filename, expected, G_N_ELEMENTS (expected));
    g_unlink (filename);
}

static void
test_remove_all_matches (void)
{
    const AuthorityRecord *input[] = {
        &matching_record_1,
        &matching_record_2,
        &other_authorization_record,
        &unrelated_record
    };
    const AuthorityRecord *expected[] = {
        &other_authorization_record,
        &unrelated_record
    };
    g_autofree gchar *filename = create_authority_file (input, G_N_ELEMENTS (input));
    g_autoptr(XAuthority) authority = create_new_authority ();
    g_autoptr(GError) error = NULL;

    g_assert_true (x_authority_write (authority, XAUTH_WRITE_MODE_REMOVE, filename, &error));
    g_assert_no_error (error);
    assert_records (filename, expected, G_N_ELEMENTS (expected));
    g_unlink (filename);
}

static void
test_remove_missing_does_not_append (void)
{
    const AuthorityRecord *input[] = {
        &other_authorization_record,
        &unrelated_record
    };
    g_autofree gchar *filename = create_authority_file (input, G_N_ELEMENTS (input));
    g_autoptr(XAuthority) authority = create_new_authority ();
    g_autoptr(GError) error = NULL;

    g_assert_true (x_authority_write (authority, XAUTH_WRITE_MODE_REMOVE, filename, &error));
    g_assert_no_error (error);
    assert_records (filename, input, G_N_ELEMENTS (input));
    g_unlink (filename);
}

static void
test_set_replaces_file (void)
{
    const AuthorityRecord *input[] = {
        &matching_record_1,
        &unrelated_record
    };
    const AuthorityRecord *expected[] = { &new_record };
    g_autofree gchar *filename = create_authority_file (input, G_N_ELEMENTS (input));
    g_autoptr(XAuthority) authority = create_new_authority ();
    g_autoptr(GError) error = NULL;

    g_assert_true (x_authority_write (authority, XAUTH_WRITE_MODE_SET, filename, &error));
    g_assert_no_error (error);
    assert_records (filename, expected, G_N_ELEMENTS (expected));
    g_unlink (filename);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/x-authority/write/replace-deduplicates", test_replace_deduplicates);
    g_test_add_func ("/x-authority/write/replace-missing-appends", test_replace_missing_appends);
    g_test_add_func ("/x-authority/write/remove-all-matches", test_remove_all_matches);
    g_test_add_func ("/x-authority/write/remove-missing-does-not-append", test_remove_missing_does_not_append);
    g_test_add_func ("/x-authority/write/set-replaces-file", test_set_replaces_file);

    return g_test_run ();
}
