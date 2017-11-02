/* Copyright StrongLoop, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include "util.h"

#define DEFAULT_CONF_PATH "/etc/ssr-native/config.json"

#if HAVE_UNISTD_H
#include <unistd.h>  /* getopt */
#endif

#define SECONDS_PER_MINUTE    1000

#define DEFAULT_BIND_HOST     "127.0.0.1"
#define DEFAULT_BIND_PORT     1080
#define DEFAULT_IDLE_TIMEOUT  (60 * SECONDS_PER_MINUTE)

static struct server_config * config_create(void);
static void config_release(struct server_config *cf);
static void parse_opts(struct server_config *cf, int argc, char **argv);
static bool parse_config_file(const char *file, struct server_config *cf);
static void usage(void);

int main(int argc, char **argv) {
    struct server_config *config;
    int err;

    _setprogname(argv[0]);

    config = config_create();
    parse_opts(config, argc, argv);

    err = listener_run(config, uv_default_loop());

    config_release(config);
    if (err) {
        exit(1);
    }

    return 0;
}

static struct server_config * config_create(void) {
    struct server_config *config;

    config = (struct server_config *) calloc(1, sizeof(*config));
    string_safe_assign(&config->listen_host, DEFAULT_BIND_HOST);
    config->listen_port = DEFAULT_BIND_PORT;
    config->idle_timeout = DEFAULT_IDLE_TIMEOUT;

    return config;
}

static void config_release(struct server_config *cf) {
    object_safe_free(&cf->listen_host);
    object_safe_free(&cf->remote_host);
    object_safe_free(&cf->password);
    object_safe_free(&cf->method);
    object_safe_free(&cf->protocol);
    object_safe_free(&cf->protocol_param);
    object_safe_free(&cf->obfs);
    object_safe_free(&cf->obfs_param);

    object_safe_free(&cf);
}

static void parse_opts(struct server_config *cf, int argc, char **argv) {
    int opt;

    while (-1 != (opt = getopt(argc, argv, "c:h"))) {
        switch (opt) {
        case 'c':
            if (parse_config_file(optarg, cf) == false) {
                usage();
            }
            break;
        case 'h':
        default:
            usage();
        }
    }
}

bool json_iter_extract_string(const char *key, const struct json_object_iter *iter, const char **value) {
    bool result = false;
    do {
        if (key == NULL || iter == NULL || value==NULL) {
            break;
        }
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        struct json_object *val = iter->val;
        if (json_type_string != json_object_get_type(val)) {
            break;
        }
        *value = json_object_get_string(val);
        result = true;
    } while (0);
    return result;
}

bool json_iter_extract_int(const char *key, const struct json_object_iter *iter, int *value) {
    bool result = false;
    do {
        if (key == NULL || iter == NULL || value==NULL) {
            break;
        }
        if (strcmp(iter->key, key) != 0) {
            break;
        }
        struct json_object *val = iter->val;
        if (json_type_int != json_object_get_type(val)) {
            break;
        }
        *value = json_object_get_int(val);
        result = true;
    } while (0);
    return result;
}

static bool parse_config_file(const char *file, struct server_config *cf) {
    bool result = false;
    json_object *jso = NULL;
    do {
        jso = json_object_from_file(file);
        if (jso == NULL) {
            break;
        }
        struct json_object_iter iter;
        json_object_object_foreachC(jso, iter) {
            int obj_int = 0;
            const char *obj_str = NULL;
            if (json_iter_extract_string("local_address", &iter, &obj_str)) {
                string_safe_assign(&cf->listen_host, obj_str);
                continue;
            }
            if (json_iter_extract_int("local_port", &iter, &obj_int)) {
                cf->listen_port = obj_int;
                continue;
            }
            if (json_iter_extract_string("server", &iter, &obj_str)) {
                string_safe_assign(&cf->remote_host, obj_str);
                continue;
            }
            if (json_iter_extract_int("server_port", &iter, &obj_int)) {
                cf->remote_port = obj_int;
                continue;
            }
            if (json_iter_extract_string("password", &iter, &obj_str)) {
                string_safe_assign(&cf->password, obj_str);
                continue;
            }
            if (json_iter_extract_string("method", &iter, &obj_str)) {
                string_safe_assign(&cf->method, obj_str);
                continue;
            }
            if (json_iter_extract_string("protocol", &iter, &obj_str)) {
                string_safe_assign(&cf->protocol, obj_str);
                continue;
            }
            if (json_iter_extract_string("protocol_param", &iter, &obj_str)) {
                string_safe_assign(&cf->protocol_param, obj_str);
                continue;
            }
            if (json_iter_extract_string("obfs", &iter, &obj_str)) {
                string_safe_assign(&cf->obfs, obj_str);
                continue;
            }
            if (json_iter_extract_string("obfs_param", &iter, &obj_str)) {
                string_safe_assign(&cf->obfs_param, obj_str);
                continue;
            }
            if (json_iter_extract_int("timeout", &iter, &obj_int)) {
                cf->idle_timeout = obj_int * SECONDS_PER_MINUTE;
                continue;
            }
        }
        result = true;
    } while (0);
    if (jso) {
        json_object_put(jso);
    }
    return result;
}

static void usage(void) {
    printf("Usage:\n"
        "\n"
        "  %s -c <config file> [-h]\n"
        "\n"
        "Options:\n"
        "\n"
        "  -c <config file>       Configure file path.\n"
        "                         Default: " DEFAULT_CONF_PATH "\n"
        "  -h                     Show this help message.\n"
        "",
        _getprogname());
    exit(1);
}
