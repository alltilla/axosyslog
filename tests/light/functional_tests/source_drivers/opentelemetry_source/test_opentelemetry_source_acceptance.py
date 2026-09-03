#!/usr/bin/env python
#############################################################################
# Copyright (c) 2025 Axoflow
# Copyright (c) 2025 Attila Szakacs <attila.szakacs@axoflow.com>
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
import base64
import json
import typing

import pytest
from axosyslog_light.syslog_ng.syslog_ng import SyslogNg
from axosyslog_light.syslog_ng_config.statements.sources.opentelemetry_source import OTelLog
from axosyslog_light.syslog_ng_config.statements.sources.opentelemetry_source import OTelResource
from axosyslog_light.syslog_ng_config.statements.sources.opentelemetry_source import OTelResourceScopeLog
from axosyslog_light.syslog_ng_config.statements.sources.opentelemetry_source import OTelScope
from axosyslog_light.syslog_ng_config.syslog_ng_config import SyslogNgConfig
from opentelemetry.proto.common.v1.common_pb2 import AnyValue
from opentelemetry.proto.metrics.v1.metrics_pb2 import AGGREGATION_TEMPORALITY_CUMULATIVE
from opentelemetry.proto.metrics.v1.metrics_pb2 import AGGREGATION_TEMPORALITY_DELTA
from opentelemetry.proto.metrics.v1.metrics_pb2 import Metric
from opentelemetry.proto.trace.v1.trace_pb2 import Span
from opentelemetry.proto.trace.v1.trace_pb2 import Status


RESOURCE_1 = OTelResource(
    attributes={
        "string": "resource_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "bytes": b"resource_1",
        "null": None,
        "list": ["resource_1_array"],
        "dict": {"key": "resource_1_value"},
    },
)
RESOURCE_1_OUTPUT = {
    "attributes": {
        "string": "resource_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "null": None,
        "bytes": base64.b64encode(b"resource_1").decode("utf-8"),
        "list": ["resource_1_array"],
        "dict": {"key": "resource_1_value"},
    },
}

RESOURCE_2 = OTelResource(
    attributes={
        "string": "resource_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": b"resource_2",
        "list": ["resource_2_array"],
        "dict": {"key": "resource_2_value"},
    },
)
RESOURCE_2_OUTPUT = {
    "attributes": {
        "string": "resource_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": base64.b64encode(b"resource_2").decode("utf-8"),
        "list": ["resource_2_array"],
        "dict": {"key": "resource_2_value"},
    },
}

SCOPE_1 = OTelScope(
    name="scope_1",
    version="1.0",
    attributes={
        "string": "scope_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "null": None,
        "bytes": b"scope_1",
        "list": ["scope_1_array"],
        "dict": {"key": "scope_1_value"},
    },
)
SCOPE_1_OUTPUT = {
    "name": "scope_1",
    "version": "1.0",
    "attributes": {
        "string": "scope_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "null": None,
        "bytes": base64.b64encode(b"scope_1").decode("utf-8"),
        "list": ["scope_1_array"],
        "dict": {"key": "scope_1_value"},
    },
}

SCOPE_2 = OTelScope(
    name="scope_2",
    version="2.0",
    attributes={
        "string": "scope_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": b"scope_2",
        "list": ["scope_2_array"],
        "dict": {"key": "scope_2_value"},
    },
)
SCOPE_2_OUTPUT = {
    "name": "scope_2",
    "version": "2.0",
    "attributes": {
        "string": "scope_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": base64.b64encode(b"scope_2").decode("utf-8"),
        "list": ["scope_2_array"],
        "dict": {"key": "scope_2_value"},
    },
}

LOG_1 = OTelLog(
    time_unix_nano=1111111111000000000,
    observed_time_unix_nano=2222222222000000000,
    severity_number=3,
    severity_text="three",
    body="log_1",
    attributes={
        "string": "log_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "null": None,
        "bytes": b"log_1",
        "list": ["log_1_array"],
        "dict": {"key": "log_1_value"},
    },
    flags=4,
    trace_id=b"trace_1",
    span_id=b"span_1",
)
LOG_1_OUTPUT = {
    "time_unix_nano": '1111111111.000000',
    "observed_time_unix_nano": '2222222222.000000',
    "severity_number": 3,
    "severity_text": "three",
    "body": "log_1",
    "attributes": {
        "string": "log_1_string",
        "int": 1,
        "bool": True,
        "double": 1.1,
        "null": None,
        "bytes": base64.b64encode(b"log_1").decode("utf-8"),
        "list": ["log_1_array"],
        "dict": {"key": "log_1_value"},
    },
    "flags": 4,
    "trace_id": base64.b64encode(b"trace_1").decode("utf-8"),
    "span_id": base64.b64encode(b"span_1").decode("utf-8"),
}

LOG_2 = OTelLog(
    time_unix_nano=5555555555000000000,
    observed_time_unix_nano=6666666666000000000,
    severity_number=7,
    severity_text="seven",
    body="log_2",
    attributes={
        "string": "log_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": b"log_2",
        "list": ["log_2_array"],
        "dict": {"key": "log_2_value"},
    },
    flags=8,
    trace_id=b"trace_2",
    span_id=b"span_2",
)
LOG_2_OUTPUT = {
    "time_unix_nano": '5555555555.000000',
    "observed_time_unix_nano": '6666666666.000000',
    "severity_number": 7,
    "severity_text": "seven",
    "body": "log_2",
    "attributes": {
        "string": "log_2_string",
        "int": 2,
        "bool": False,
        "double": 2.2,
        "null": None,
        "bytes": base64.b64encode(b"log_2").decode("utf-8"),
        "list": ["log_2_array"],
        "dict": {"key": "log_2_value"},
    },
    "flags": 8,
    "trace_id": base64.b64encode(b"trace_2").decode("utf-8"),
    "span_id": base64.b64encode(b"span_2").decode("utf-8"),
}


FILTERX = r"""
    resource = otel_resource(${.otel_raw.resource});
    scope = otel_scope(${.otel_raw.scope});
    log = otel_logrecord(${.otel_raw.log});

    $MSG = {
        "resource": resource,
        "scope": scope,
        "log": log,
    };
"""

TEMPLATE = '"$MSG\n"'


@pytest.mark.parametrize(
    "resource, resource_output, scope, scope_output, log, log_output",
    [
        (RESOURCE_1, RESOURCE_1_OUTPUT, SCOPE_1, SCOPE_1_OUTPUT, LOG_1, LOG_1_OUTPUT),
        (RESOURCE_1, RESOURCE_1_OUTPUT, None, dict(), LOG_1, LOG_1_OUTPUT),
        (None, dict(), SCOPE_1, SCOPE_1_OUTPUT, LOG_1, LOG_1_OUTPUT),
        (None, dict(), None, dict(), LOG_1, LOG_1_OUTPUT),
        (RESOURCE_1, RESOURCE_1_OUTPUT, SCOPE_1, SCOPE_1_OUTPUT, None, {"body": None}),
        (RESOURCE_1, RESOURCE_1_OUTPUT, None, dict(), None, {"body": None}),
        (None, dict(), SCOPE_1, SCOPE_1_OUTPUT, None, {"body": None}),
        (None, dict(), None, dict(), None, {"body": None}),
    ],
    ids=[
        "w_resource_w_scope_w_log",
        "w_resource_wo_scope_w_log",
        "wo_resource_w_scope_w_log",
        "wo_resource_wo_scope_w_log",
        "w_resource_w_scope_wo_log",
        "w_resource_wo_scope_wo_log",
        "wo_resource_w_scope_wo_log",
        "wo_resource_wo_scope_wo_log",
    ],
)
def test_opentelemetry_source_acceptance_single_log(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
    resource: typing.Optional[OTelResource],
    resource_output: typing.Dict[str, typing.Any],
    scope: typing.Optional[OTelScope],
    scope_output: typing.Dict[str, typing.Any],
    log: typing.Optional[OTelLog],
    log_output: typing.Optional[OTelScope],
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator())
    filterx = config.create_filterx(FILTERX)
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_log(resource=resource, scope=scope, log=log)
    assert json.loads(file_destination.read_log()) == {
        "resource": resource_output,
        "scope": scope_output,
        "log": log_output,
    }


def test_opentelemetry_source_acceptance_batch(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator())
    filterx = config.create_filterx(FILTERX)
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    logs: typing.List[OTelResourceScopeLog] = [
        OTelResourceScopeLog(RESOURCE_1, SCOPE_1, LOG_1),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_1, LOG_1),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_1, LOG_2),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_1, LOG_2),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_2, LOG_1),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_2, LOG_1),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_2, LOG_2),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_2, LOG_2),
    ]
    expected_logs = [
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_2_OUTPUT},
    ]

    syslog_ng.start(config)
    opentelemetry_source.write_logs(logs)

    for expected_log in expected_logs:
        assert json.loads(file_destination.read_log()) == expected_log


@pytest.mark.parametrize(
    "resource, resource_output, scope, scope_output, log, log_output",
    [
        (RESOURCE_1, RESOURCE_1_OUTPUT, SCOPE_1, SCOPE_1_OUTPUT, LOG_1, LOG_1_OUTPUT),
        (RESOURCE_1, RESOURCE_1_OUTPUT, None, dict(), LOG_1, LOG_1_OUTPUT),
        (None, dict(), SCOPE_1, SCOPE_1_OUTPUT, LOG_1, LOG_1_OUTPUT),
        (None, dict(), None, dict(), LOG_1, LOG_1_OUTPUT),
        (RESOURCE_1, RESOURCE_1_OUTPUT, SCOPE_1, SCOPE_1_OUTPUT, None, {"body": None}),
        (RESOURCE_1, RESOURCE_1_OUTPUT, None, dict(), None, {"body": None}),
        (None, dict(), SCOPE_1, SCOPE_1_OUTPUT, None, {"body": None}),
        (None, dict(), None, dict(), None, {"body": None}),
    ],
    ids=[
        "w_resource_w_scope_w_log",
        "w_resource_wo_scope_w_log",
        "wo_resource_w_scope_w_log",
        "wo_resource_wo_scope_w_log",
        "w_resource_w_scope_wo_log",
        "w_resource_wo_scope_wo_log",
        "wo_resource_w_scope_wo_log",
        "wo_resource_wo_scope_wo_log",
    ],
)
def test_opentelemetry_source_filterx_dict_mode_single_log(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
    resource: typing.Optional[OTelResource],
    resource_output: typing.Dict[str, typing.Any],
    scope: typing.Optional[OTelScope],
    scope_output: typing.Dict[str, typing.Any],
    log: typing.Optional[OTelLog],
    log_output: typing.Dict[str, typing.Any],
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    filterx = config.create_filterx(r"""
        $MSG = {
            "resource": resource,
            "scope": scope,
            "log": log,
        };""")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_log(resource=resource, scope=scope, log=log)
    assert json.loads(file_destination.read_log()) == {
        "resource": resource_output,
        "scope": scope_output,
        "log": log_output,
    }


def test_opentelemetry_source_filterx_dict_mode_batch(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    filterx = config.create_filterx(r"""
        $MSG = {
            "resource": resource,
            "scope": scope,
            "log": log,
        };""")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    logs: typing.List[OTelResourceScopeLog] = [
        OTelResourceScopeLog(RESOURCE_1, SCOPE_1, LOG_1),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_1, LOG_1),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_1, LOG_2),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_1, LOG_2),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_2, LOG_1),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_2, LOG_1),
        OTelResourceScopeLog(RESOURCE_1, SCOPE_2, LOG_2),
        OTelResourceScopeLog(RESOURCE_2, SCOPE_2, LOG_2),
    ]
    expected_logs = [
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_1_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_1_OUTPUT, "log": LOG_2_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_1_OUTPUT},
        {"resource": RESOURCE_2_OUTPUT, "scope": SCOPE_2_OUTPUT, "log": LOG_2_OUTPUT},
    ]

    syslog_ng.start(config)
    opentelemetry_source.write_logs(logs)

    for expected_log in expected_logs:
        assert json.loads(file_destination.read_log()) == expected_log


def _metric_gauge() -> Metric:
    metric = Metric(name="gauge", description="d", unit="1")
    data_point = metric.gauge.data_points.add()
    data_point.time_unix_nano = 1111111111000000000
    data_point.as_double = 1.5
    data_point.attributes.add(key="a", value=AnyValue(string_value="b"))
    return metric


def _metric_sum() -> Metric:
    metric = Metric(name="sum")
    data_point = metric.sum.data_points.add()
    data_point.start_time_unix_nano = 1111111111000000000
    data_point.time_unix_nano = 2222222222000000000
    data_point.as_int = 5
    metric.sum.aggregation_temporality = AGGREGATION_TEMPORALITY_CUMULATIVE
    metric.sum.is_monotonic = True
    return metric


def _metric_histogram() -> Metric:
    metric = Metric(name="histogram")
    data_point = metric.histogram.data_points.add()
    data_point.time_unix_nano = 1111111111000000000
    data_point.count = 3
    data_point.sum = 6.5
    data_point.bucket_counts.extend([1, 2])
    data_point.explicit_bounds.extend([0.5])
    data_point.min = 0.5
    data_point.max = 2.5
    metric.histogram.aggregation_temporality = AGGREGATION_TEMPORALITY_DELTA
    return metric


def _metric_exponential_histogram() -> Metric:
    metric = Metric(name="exponential_histogram")
    data_point = metric.exponential_histogram.data_points.add()
    data_point.time_unix_nano = 1111111111000000000
    data_point.count = 4
    data_point.scale = -1
    data_point.zero_count = 1
    data_point.positive.offset = 2
    data_point.positive.bucket_counts.extend([1, 1])
    data_point.negative.offset = -3
    data_point.negative.bucket_counts.extend([1])
    metric.exponential_histogram.aggregation_temporality = AGGREGATION_TEMPORALITY_CUMULATIVE
    return metric


def _metric_summary() -> Metric:
    metric = Metric(name="summary")
    data_point = metric.summary.data_points.add()
    data_point.time_unix_nano = 1111111111000000000
    data_point.count = 2
    data_point.sum = 3.5
    data_point.quantile_values.add(quantile=0.5, value=1.5)
    data_point.quantile_values.add(quantile=0.75, value=2.5)
    return metric


@pytest.mark.parametrize(
    "metric, metric_output",
    [
        (
            _metric_gauge(),
            {
                "name": "gauge", "description": "d", "unit": "1",
                "gauge": {"data_points": [{"time_unix_nano": "1111111111.000000", "as_double": 1.5, "attributes": {"a": "b"}}]},
            },
        ),
        (
            _metric_sum(),
            {
                "name": "sum",
                "sum": {
                    "data_points": [{"start_time_unix_nano": "1111111111.000000", "time_unix_nano": "2222222222.000000", "as_int": 5}],
                    "aggregation_temporality": 2, "is_monotonic": True,
                },
            },
        ),
        (
            _metric_histogram(),
            {
                "name": "histogram",
                "histogram": {
                    "data_points": [{
                        "time_unix_nano": "1111111111.000000", "count": 3, "sum": 6.5,
                        "bucket_counts": [1, 2], "explicit_bounds": [0.5], "min": 0.5, "max": 2.5,
                    }],
                    "aggregation_temporality": 1,
                },
            },
        ),
        (
            _metric_exponential_histogram(),
            {
                "name": "exponential_histogram",
                "exponential_histogram": {
                    "data_points": [{
                        "time_unix_nano": "1111111111.000000", "count": 4, "scale": -1, "zero_count": 1,
                        "positive": {"offset": 2, "bucket_counts": [1, 1]}, "negative": {"offset": -3, "bucket_counts": [1]},
                    }],
                    "aggregation_temporality": 2,
                },
            },
        ),
        (
            _metric_summary(),
            {
                "name": "summary",
                "summary": {
                    "data_points": [{
                        "time_unix_nano": "1111111111.000000", "count": 2, "sum": 3.5,
                        "quantile_values": [{"quantile": 0.5, "value": 1.5}, {"quantile": 0.75, "value": 2.5}],
                    }],
                },
            },
        ),
    ],
    ids=["gauge", "sum", "histogram", "exponential_histogram", "summary"],
)
def test_opentelemetry_source_filterx_dict_mode_metric(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
    metric: Metric,
    metric_output: typing.Dict[str, typing.Any],
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    filterx = config.create_filterx(r"""
        $MSG = {
            "type": ${.otel_raw.type},
            "has_log": isset(log),
            "resource": resource,
            "scope": scope,
            "metric": metric,
        };""")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_metric(resource=RESOURCE_1, scope=SCOPE_1, metric=metric)
    assert json.loads(file_destination.read_log()) == {
        "type": "metric",
        "has_log": False,
        "resource": RESOURCE_1_OUTPUT,
        "scope": SCOPE_1_OUTPUT,
        "metric": metric_output,
    }


def _span() -> Span:
    span = Span(
        trace_id=b"\x01\x02",
        span_id=b"\x03",
        trace_state="k=v",
        parent_span_id=b"\x04",
        name="GET /",
        kind=Span.SPAN_KIND_SERVER,
        start_time_unix_nano=1111111111000000000,
        end_time_unix_nano=2222222222000000000,
    )
    span.attributes.add(key="http.method", value=AnyValue(string_value="GET"))
    event = span.events.add(time_unix_nano=1111111111500000000, name="ev")
    event.attributes.add(key="e", value=AnyValue(bool_value=True))
    link = span.links.add(trace_id=b"\x05", span_id=b"\x06", trace_state="x=y")
    link.attributes.add(key="l", value=AnyValue(int_value=2))
    span.status.message = "boom"
    span.status.code = Status.STATUS_CODE_ERROR
    return span


SPAN_OUTPUT = {
    "trace_id": base64.b64encode(b"\x01\x02").decode("utf-8"),
    "span_id": base64.b64encode(b"\x03").decode("utf-8"),
    "trace_state": "k=v",
    "parent_span_id": base64.b64encode(b"\x04").decode("utf-8"),
    "name": "GET /",
    "kind": 2,
    "start_time_unix_nano": "1111111111.000000",
    "end_time_unix_nano": "2222222222.000000",
    "attributes": {"http.method": "GET"},
    "events": [{"time_unix_nano": "1111111111.500000", "name": "ev", "attributes": {"e": True}}],
    "links": [
        {
            "trace_id": base64.b64encode(b"\x05").decode("utf-8"),
            "span_id": base64.b64encode(b"\x06").decode("utf-8"),
            "trace_state": "x=y",
            "attributes": {"l": 2},
        },
    ],
    "status": {"message": "boom", "code": 2},
}


def test_opentelemetry_source_filterx_dict_mode_span(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    filterx = config.create_filterx(r"""
        $MSG = {
            "type": ${.otel_raw.type},
            "has_metric": isset(metric),
            "resource": resource,
            "scope": scope,
            "span": span,
        };""")
    file_destination = config.create_file_destination(file_name="output.log", template=TEMPLATE)
    config.create_logpath(statements=[opentelemetry_source, filterx, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_span(resource=RESOURCE_1, scope=SCOPE_1, span=_span())
    assert json.loads(file_destination.read_log()) == {
        "type": "span",
        "has_metric": False,
        "resource": RESOURCE_1_OUTPUT,
        "scope": SCOPE_1_OUTPUT,
        "span": SPAN_OUTPUT,
    }


def test_opentelemetry_source_filterx_dict_mode_sets_raw_type_and_schema_urls(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    file_destination = config.create_file_destination(
        file_name="output.log",
        template='"${.otel_raw.type} ${.otel_raw.resource_schema_url} ${.otel_raw.scope_schema_url}\\n"',
    )
    config.create_logpath(statements=[opentelemetry_source, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_log(
        resource=OTelResource(schema_url="https://example.com/resource"),
        scope=OTelScope(schema_url="https://example.com/scope"),
        log=LOG_1,
    )

    assert file_destination.read_log() == "log https://example.com/resource https://example.com/scope"


def test_opentelemetry_source_filterx_dict_mode_sets_peer_address(
    syslog_ng: SyslogNg,
    config: SyslogNgConfig,
    port_allocator,
) -> None:
    opentelemetry_source = config.create_opentelemetry_source(port=port_allocator(), mode="filterx-dict")
    file_destination = config.create_file_destination(file_name="output.log", template='"$SOURCEIP $IP_PROTO\n"')
    config.create_logpath(statements=[opentelemetry_source, file_destination])

    syslog_ng.start(config)
    opentelemetry_source.write_log(log=LOG_1)

    # $SOURCEIP alone cannot catch a missing source address, it falls back
    # to 127.0.0.1, which is also the real peer address here; $IP_PROTO is
    # 0 when the source address is unset
    sourceip, ip_proto = file_destination.read_log().split()
    assert sourceip in ("127.0.0.1", "::1")
    assert ip_proto in ("4", "6")
