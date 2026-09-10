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
import typing
from dataclasses import dataclass
from dataclasses import field
from pathlib import Path

from grpc import insecure_channel
from grpc import secure_channel
from grpc import ssl_channel_credentials
from opentelemetry.proto.collector.logs.v1.logs_service_pb2 import ExportLogsServiceRequest
from opentelemetry.proto.collector.logs.v1.logs_service_pb2_grpc import LogsServiceStub
from opentelemetry.proto.collector.metrics.v1.metrics_service_pb2 import ExportMetricsServiceRequest
from opentelemetry.proto.collector.metrics.v1.metrics_service_pb2_grpc import MetricsServiceStub
from opentelemetry.proto.collector.trace.v1.trace_service_pb2 import ExportTraceServiceRequest
from opentelemetry.proto.collector.trace.v1.trace_service_pb2_grpc import TraceServiceStub
from opentelemetry.proto.common.v1.common_pb2 import AnyValue
from opentelemetry.proto.common.v1.common_pb2 import ArrayValue
from opentelemetry.proto.common.v1.common_pb2 import InstrumentationScope
from opentelemetry.proto.common.v1.common_pb2 import KeyValue
from opentelemetry.proto.common.v1.common_pb2 import KeyValueList
from opentelemetry.proto.logs.v1.logs_pb2 import LogRecord
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_DEBUG
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_DEBUG2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_DEBUG3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_DEBUG4
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_ERROR
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_ERROR2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_ERROR3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_ERROR4
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_FATAL
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_FATAL2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_FATAL3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_FATAL4
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_INFO
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_INFO2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_INFO3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_INFO4
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_TRACE
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_TRACE2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_TRACE3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_TRACE4
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_UNSPECIFIED
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_WARN
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_WARN2
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_WARN3
from opentelemetry.proto.logs.v1.logs_pb2 import SEVERITY_NUMBER_WARN4
from opentelemetry.proto.metrics.v1.metrics_pb2 import Metric
from opentelemetry.proto.resource.v1.resource_pb2 import Resource
from opentelemetry.proto.trace.v1.trace_pb2 import Span


class PyToOTelConverter:
    @staticmethod
    def convert_value_to_any_value(value: typing.Any) -> AnyValue:
        if isinstance(value, str):
            return AnyValue(string_value=value)
        if isinstance(value, bool):
            return AnyValue(bool_value=value)
        if isinstance(value, int):
            return AnyValue(int_value=value)
        if isinstance(value, float):
            return AnyValue(double_value=value)
        if isinstance(value, bytes):
            return AnyValue(bytes_value=value)
        if isinstance(value, dict):
            return AnyValue(kvlist_value=PyToOTelConverter.convert_dict_to_key_value_list(value))
        if isinstance(value, list):
            return AnyValue(array_value=PyToOTelConverter.convert_list_to_values(value))
        if isinstance(value, type(None)):
            return AnyValue()
        raise ValueError("Unsupported value type: {}".format(type(value)))

    @staticmethod
    def convert_dict_to_key_value_list(data: typing.Dict[str, typing.Any]) -> KeyValueList:
        key_value_list = KeyValueList()

        for key, value in data.items():
            if not isinstance(key, str):
                raise ValueError("Key must be a string: {}".format(key))

            key_value = KeyValue()
            key_value.key = key
            key_value.value.CopyFrom(PyToOTelConverter.convert_value_to_any_value(value))
            key_value_list.values.append(key_value)

        return key_value_list

    @staticmethod
    def convert_list_to_values(data: typing.List[typing.Any]) -> ArrayValue:
        array_value = ArrayValue()

        for value in data:
            array_value.values.append(PyToOTelConverter.convert_value_to_any_value(value))

        return array_value


@dataclass
class OTelResource:
    attributes: typing.Dict[str, typing.Any] = field(default_factory=dict)
    schema_url: str = ""

    def to_otel(self) -> Resource:
        resource = Resource()
        resource.attributes.extend(PyToOTelConverter.convert_dict_to_key_value_list(self.attributes).values)
        return resource


@dataclass
class OTelScope:
    name: str = ""
    version: str = ""
    attributes: typing.Dict[str, typing.Any] = field(default_factory=dict)
    schema_url: str = ""

    def to_otel(self) -> InstrumentationScope:
        scope = InstrumentationScope()
        scope.name = self.name
        scope.version = self.version
        scope.attributes.extend(PyToOTelConverter.convert_dict_to_key_value_list(self.attributes).values)
        return scope


@dataclass
class OTelLog:
    time_unix_nano: int = 0
    observed_time_unix_nano: int = 0
    severity_number: int = 0
    severity_text: str = ""
    body: typing.Any = None
    attributes: typing.Dict[str, typing.Any] = field(default_factory=dict)
    flags: int = 0
    trace_id: bytes = b""
    span_id: bytes = b""

    def to_otel(self) -> LogRecord:
        SEVERITY_NUMBERS = [
            SEVERITY_NUMBER_UNSPECIFIED,
            SEVERITY_NUMBER_TRACE,
            SEVERITY_NUMBER_TRACE2,
            SEVERITY_NUMBER_TRACE3,
            SEVERITY_NUMBER_TRACE4,
            SEVERITY_NUMBER_DEBUG,
            SEVERITY_NUMBER_DEBUG2,
            SEVERITY_NUMBER_DEBUG3,
            SEVERITY_NUMBER_DEBUG4,
            SEVERITY_NUMBER_INFO,
            SEVERITY_NUMBER_INFO2,
            SEVERITY_NUMBER_INFO3,
            SEVERITY_NUMBER_INFO4,
            SEVERITY_NUMBER_WARN,
            SEVERITY_NUMBER_WARN2,
            SEVERITY_NUMBER_WARN3,
            SEVERITY_NUMBER_WARN4,
            SEVERITY_NUMBER_ERROR,
            SEVERITY_NUMBER_ERROR2,
            SEVERITY_NUMBER_ERROR3,
            SEVERITY_NUMBER_ERROR4,
            SEVERITY_NUMBER_FATAL,
            SEVERITY_NUMBER_FATAL2,
            SEVERITY_NUMBER_FATAL3,
            SEVERITY_NUMBER_FATAL4,
        ]

        log = LogRecord()
        log.time_unix_nano = self.time_unix_nano
        log.observed_time_unix_nano = self.observed_time_unix_nano
        log.severity_number = SEVERITY_NUMBERS[self.severity_number]
        log.severity_text = self.severity_text
        log.body.CopyFrom(PyToOTelConverter.convert_value_to_any_value(self.body))
        log.attributes.extend(PyToOTelConverter.convert_dict_to_key_value_list(self.attributes).values)
        log.flags = self.flags
        log.trace_id = self.trace_id
        log.span_id = self.span_id
        return log


class OTelResourceScopeLog:
    def __init__(
        self,
        resource: typing.Optional[OTelResource] = None,
        scope: typing.Optional[OTelScope] = None,
        log: typing.Optional[OTelLog] = None,
    ) -> None:
        self.resource = resource if resource is not None else OTelResource()
        self.scope = scope if scope is not None else OTelScope()
        self.log = log if log is not None else OTelLog()


class OTelResourceScopeMetric:
    def __init__(
        self,
        resource: typing.Optional[OTelResource] = None,
        scope: typing.Optional[OTelScope] = None,
        metric: typing.Optional[Metric] = None,
    ) -> None:
        self.resource = resource if resource is not None else OTelResource()
        self.scope = scope if scope is not None else OTelScope()
        self.metric = metric if metric is not None else Metric()


class OTelResourceScopeSpan:
    def __init__(
        self,
        resource: typing.Optional[OTelResource] = None,
        scope: typing.Optional[OTelScope] = None,
        span: typing.Optional[Span] = None,
    ) -> None:
        self.resource = resource if resource is not None else OTelResource()
        self.scope = scope if scope is not None else OTelScope()
        self.span = span if span is not None else Span()


class OpenTelemetryIO():
    def __init__(self, port: int, address: str = "127.0.0.1") -> None:
        self.__port = port
        self.__address = address
        self.__credentials = None

    def set_tls(
        self,
        ca_cert: Path,
        client_cert: typing.Optional[Path] = None,
        client_key: typing.Optional[Path] = None,
    ) -> None:
        self.__credentials = ssl_channel_credentials(
            root_certificates=ca_cert.read_bytes(),
            private_key=client_key.read_bytes() if client_key else None,
            certificate_chain=client_cert.read_bytes() if client_cert else None,
        )

    def __create_channel(self):
        target = f"{self.__address}:{self.__port}"
        if self.__credentials:
            return secure_channel(target, self.__credentials)
        return insecure_channel(target)

    @staticmethod
    def __group_by_resource_and_scope(
        request: typing.Any,
        items: typing.List[typing.Any],
        resource_group_field: str,
        scope_group_field: str,
        signal_field: str,
        signal_to_otel: typing.Callable[[typing.Any], typing.Any],
    ) -> typing.Any:
        for item in items:
            resource = item.resource.to_otel()
            resource_group = next(
                (
                    group for group in getattr(request, resource_group_field)
                    if group.resource == resource and group.schema_url == item.resource.schema_url
                ),
                None,
            )
            if resource_group is None:
                resource_group = getattr(request, resource_group_field).add()
                resource_group.resource.CopyFrom(resource)
                resource_group.schema_url = item.resource.schema_url

            scope = item.scope.to_otel()
            scope_group = next(
                (
                    group for group in getattr(resource_group, scope_group_field)
                    if group.scope == scope and group.schema_url == item.scope.schema_url
                ),
                None,
            )
            if scope_group is None:
                scope_group = getattr(resource_group, scope_group_field).add()
                scope_group.scope.CopyFrom(scope)
                scope_group.schema_url = item.scope.schema_url

            getattr(scope_group, signal_field).append(signal_to_otel(item))

        return request

    def send_logs(self, resource_scope_logs: typing.List[OTelResourceScopeLog]) -> None:
        request = self.__group_by_resource_and_scope(
            ExportLogsServiceRequest(), resource_scope_logs, "resource_logs", "scope_logs", "log_records",
            lambda item: item.log.to_otel(),
        )
        with self.__create_channel() as channel:
            LogsServiceStub(channel).Export(request)

    def send_metrics(self, resource_scope_metrics: typing.List[OTelResourceScopeMetric]) -> None:
        request = self.__group_by_resource_and_scope(
            ExportMetricsServiceRequest(), resource_scope_metrics, "resource_metrics", "scope_metrics", "metrics",
            lambda item: item.metric,
        )
        with self.__create_channel() as channel:
            MetricsServiceStub(channel).Export(request)

    def send_spans(self, resource_scope_spans: typing.List[OTelResourceScopeSpan]) -> None:
        request = self.__group_by_resource_and_scope(
            ExportTraceServiceRequest(), resource_scope_spans, "resource_spans", "scope_spans", "spans",
            lambda item: item.span,
        )
        with self.__create_channel() as channel:
            TraceServiceStub(channel).Export(request)
