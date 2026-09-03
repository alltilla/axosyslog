`filterx`: Added `parse_otel_resource()`, `parse_otel_scope()`, `parse_otel_logrecord()`, `parse_otel_span()`,
`parse_otel_metric()` and their `format_otel_*()` counterparts.

`parse_otel_*()` converts a protobuf or bytes value, for example `${.otel_raw.log}` of an `opentelemetry()` source in
`mode(logmessage)`, to the same dict `mode(filterx-dict)` injects. `format_otel_*()` serializes such a dict back to
protobuf, which the `opentelemetry()` destination expects in the `${.otel_raw.<...>}` NVs:

```
${.otel_raw.resource} = format_otel_resource(resource);
${.otel_raw.scope} = format_otel_scope(scope);
${.otel_raw.log} = format_otel_logrecord(log);
```

`format_otel_*()` is strict: an unknown key, a value of the wrong type or two members of the same `oneof` fail the
call. In `mode(filterx-dict)` these calls are needed before an `opentelemetry()` destination, otherwise the
destination sends an empty record.
