# noinst: Built as part of a larger libmetrics-probe module as static library
noinst_LTLIBRARIES += modules/metrics-probe/filterx/libmetrics-probe-filterx.la

modules_metrics_probe_filterx_libmetrics_probe_filterx_la_SOURCES = \
  modules/metrics-probe/filterx/metrics-probe-filterx.c \
  modules/metrics-probe/filterx/metrics-probe-filterx.h

modules_metrics_probe_filterx_libmetrics_probe_filterx_la_CFLAGS = \
  -I$(top_srcdir)/modules/metrics-probe/filterx \
  -I$(top_builddir)/modules/metrics-probe/filterx

modules_metrics_probe_filterx_libmetrics_probe_filterx_la_LIBADD = \
  $(MODULE_DEPS_LIBS)

modules_metrics_probe_filterx_libmetrics_probe_filterx_la_LDFLAGS = \
  $(MODULE_LDFLAGS)

EXTRA_modules_metrics_probe_filterx_libmetrics_probe_filterx_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS)

EXTRA_DIST += \
  modules/metrics-probe/filterx/CMakeLists.txt
