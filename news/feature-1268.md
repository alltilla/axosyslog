`format_csv()` FilterX function: Added the `dialect` option.

`dialect="escape-double-char"` writes RFC 4180 CSV: values containing the delimiter, `"`, CR or LF are quoted and
`"` is doubled. The default, `escape-backslash-with-sequences`, keeps the previous output.
