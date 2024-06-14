set(METRICS_PROBE_FILTERX_SOURCES
  metrics-probe-filterx.c
  metrics-probe-filterx.h
)

add_module(
  TARGET metrics_probe_filterx
  SOURCES ${METRICS_PROBE_FILTERX_SOURCES}
  LIBRARY_TYPE STATIC
)
