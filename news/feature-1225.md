`opentelemetry()` source: `mode(filterx-dict)` now covers metrics and traces, too.

A metric arrives in the `metric` FilterX variable, a span in the `span` variable, next to `resource` and `scope`,
the same way a log record arrives in `log`. Use `isset(metric)` or `isset(span)` to tell the signals apart.

The source also sets `${.otel_raw.type}`, `${.otel_raw.resource_schema_url}` and `${.otel_raw.scope_schema_url}`
in this mode, only the protobuf NVs are skipped.

The `*_unix_nano` timestamps are FilterX datetime values with microsecond precision, so a round trip through a
dict drops the last three digits of a nanosecond timestamp.
