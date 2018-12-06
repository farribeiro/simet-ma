/*
 * Copyright (c) 2018 NIC.br <medicoes@simet.nic.br>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.  In every case, additional
 * restrictions and permissions apply, refer to the COPYING file in the
 * program Source for details.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License and the COPYING file in the program Source
 * for details.
 */

#include "tcpbwc_config.h"
#include "report.h"

#include "logger.h"

#include "json-c/json.h"
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

struct tcpbw_report_private {
	int placeholder;
};

static void xx_json_object_array_add_uin64_as_str(json_object *j, uint64_t v)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%" PRIu64, v);
    json_object_array_add(j, json_object_new_string(buf));
}

/**
 * createReport - create the JSON LMAP-like report snippet
 *
 * if @jresults is not NULL, include it (upload direction measurement results).
 * Then, render the rows for DownResult[] (download direction measurement results), if any.
 *
 * @jresults MUST use the same column ordering as we do:
 * sequence; bits; streams; interval (ms); direction
 */
static json_object *createReport(json_object *jresults,
			  DownResult *downloadRes, uint32_t counter,
			  MeasureContext *ctx)
{
    char metric_name[256];

    assert(downloadRes);
    assert(ctx);

    snprintf(metric_name, sizeof(metric_name),
	    "Priv_OWBTC_Active_TCP-SustainedBurst-MultipleStreams-"
	    "TCPOptsUndefined-SamplePeriodMs%u-StreamDurationMs%u000__Multiple_Raw",
	    ctx->sample_period_ms, ctx->test_duration);

    /* FIXME: handle NULL returns as error... */

    json_object *jo, *jo1, *jo2; /* used when transfering ownership via _add */

    /* shall contain function, column, row arrays */
    json_object *jtable = json_object_new_object();
    assert(jtable);

    if (!json_object_is_type(jresults, json_type_array))
    {
        print_warn("Received unusable data from server, ignoring...");
        jresults = NULL;
    }

    /* function object list */
    jo = json_object_new_array();
    jo1 = json_object_new_object();
    jo2 = json_object_new_array();
    assert(jo && jo1 && jo2);
    json_object_object_add(jo1, "uri", json_object_new_string(metric_name));
    json_object_array_add(jo2, json_object_new_string("client"));
    json_object_object_add(jo1, "role", jo2);
    json_object_array_add(jo, jo1);
    json_object_object_add(jtable, "function", jo);
    jo = jo1 = jo2 = NULL;

    /* columns list */
    jo = json_object_new_array();
    assert(jo);
    json_object_array_add(jo, json_object_new_string("sequence"));
    json_object_array_add(jo, json_object_new_string("bits"));
    json_object_array_add(jo, json_object_new_string("streams"));
    json_object_array_add(jo, json_object_new_string("intervalMs"));
    json_object_array_add(jo, json_object_new_string("direction"));
    json_object_object_add(jtable, "column", jo);
    jo = NULL;

    /* rows (result data) */
    json_object *jrows = (jresults) ? jresults : json_object_new_array();
    assert(jrows);

    for (unsigned int i = 0; i < counter; i++)
    {
        json_object *jrow = json_object_new_array();
        assert(jrow);

        /* WARNING: keep the same order as in the columns list! */
        xx_json_object_array_add_uin64_as_str(jrow, i + 1);
        xx_json_object_array_add_uin64_as_str(jrow, downloadRes[i].bytes * 8U);
        xx_json_object_array_add_uin64_as_str(jrow, downloadRes[i].nstreams);
        xx_json_object_array_add_uin64_as_str(jrow, (uint64_t)downloadRes[i].interval / 1000UL);
        json_object_array_add(jrow, json_object_new_string("download"));

        /* add row to list of rows */
        jo = json_object_new_object();
        json_object_object_add(jo, "value", jrow);
        json_object_array_add(jrows, jo);
        jo = NULL;
    }

    json_object_object_add(jtable, "row", jrows);
    jrows = NULL;

    return jtable;
}

int tcpbw_report(struct tcpbw_report *report,
                 const char *upload_results_json,
                 DownResult *downloadRes, uint32_t counter,
                 MeasureContext *ctx)
{
    assert(report && upload_results_json && downloadRes && ctx);
    json_object *j_obj_upload = json_tokener_parse(upload_results_json);
    json_object *report_obj = createReport(j_obj_upload, downloadRes, counter, ctx);
    if (report_obj) {
        fprintf(stdout, "%s\n", json_object_to_json_string_ext(report_obj,
                    JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    }

    return 0;
}

/**
 * tcpbw_report_init() - allocates and initializes a tcpbw_report struct
 *
 * Returns NULL on ENOMEM.
 */
struct tcpbw_report * tcpbw_report_init(void)
{
    /* FIXME: dummy placeholder */
    return malloc(sizeof(struct tcpbw_report_private));
}

/**
 * tcpbw_report_done - deallocates a tcpbw_report struct
 *
 * frees all substructures and private data
 *
 * Handles NULL structs just fine.
 */
void tcpbw_report_done(struct tcpbw_report *r)
{
    free(r);
}

