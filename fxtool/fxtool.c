/*
 * Copyright (c) 2026 Axoflow
 * Copyright (c) 2026 Attila Szakacs-Bertok <attila.szakacs@axoflow.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "syslog-ng.h"
#include "filterx/filterx-parser.h"
#include "cfg.h"
#include "versioning.h"
#include "apphook.h"
#include "messages.h"
#include "resolved-configurable-paths.h"
#include "scratch-buffers.h"

#include <stdio.h>
#include <string.h>

static gboolean
_compile(const gchar *script, GString **ast)
{
  /* a full config, not a snippet: the script may call functions that live in modules */
  GlobalConfig *cfg = cfg_new(VERSION_VALUE_CURRENT);
  gboolean success = filterx_compile_script_file(cfg, script, ast);

  scratch_buffers_explicit_gc();
  cfg_free(cfg);
  return success;
}

static gint
_check(gint argc, gchar *argv[])
{
  return _compile(argv[1], NULL) ? 0 : 1;
}

static gint
_ast(gint argc, gchar *argv[])
{
  GString *ast = NULL;

  if (!_compile(argv[1], &ast))
    return 1;

  printf("%s\n", ast->str);
  g_string_free(ast, TRUE);
  return 0;
}

static GOptionEntry fxtool_options[] =
{
  {
    "module-path", 0, 0, G_OPTION_ARG_STRING, &resolved_configurable_paths.initial_module_path,
    "Set the list of colon separated directories to search for modules, default=" SYSLOG_NG_MODULE_PATH, "<path>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static struct
{
  const gchar *mode;
  const gchar *description;
  gint (*main)(gint argc, gchar *argv[]);
} modes[] =
{
  { "check", "Compile a filterx script and report its errors", _check },
  { "ast", "Compile a filterx script and print its abstract syntax tree as JSON", _ast },
  { NULL, NULL, NULL },
};

static void
_usage(void)
{
  fprintf(stderr, "Syntax: fxtool <command> [options] <script>\nPossible commands are:\n");
  for (gint mode = 0; modes[mode].mode; mode++)
    fprintf(stderr, "    %-12s %s\n", modes[mode].mode, modes[mode].description);
}

/* the first argument that is not an option names the command, it is removed from argv */
static const gchar *
_extract_mode(int *argc, char **argv[])
{
  for (gint i = 1; i < *argc; i++)
    {
      if ((*argv)[i][0] != '-')
        {
          const gchar *mode = (*argv)[i];

          memmove(&(*argv)[i], &(*argv)[i + 1], (*argc - i) * sizeof(gchar *));
          (*argc)--;
          return mode;
        }
    }
  return NULL;
}

int
main(int argc, char *argv[])
{
  GOptionContext *ctx = NULL;
  GError *error = NULL;
  gint mode;

  setvbuf(stderr, NULL, _IONBF, 0);
  msg_init(TRUE);
  resolved_configurable_paths_init(&resolved_configurable_paths);

  const gchar *mode_string = _extract_mode(&argc, &argv);
  if (!mode_string)
    {
      _usage();
      return 1;
    }

  for (mode = 0; modes[mode].mode; mode++)
    {
      if (strcmp(modes[mode].mode, mode_string) == 0)
        {
          ctx = g_option_context_new("<script>");
          g_option_context_set_summary(ctx, modes[mode].description);
          g_option_context_add_main_entries(ctx, fxtool_options, NULL);
          msg_add_option_group(ctx);
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      _usage();
      return 1;
    }

  argv[0] = g_strdup_printf("%s %s", argv[0], mode_string);

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);

  if (argc != 2)
    {
      fprintf(stderr, "Syntax: %s [options] <script>\n", argv[0]);
      return 1;
    }

  app_startup();
  gint result = modes[mode].main(argc, argv);
  app_shutdown();

  return result;
}
