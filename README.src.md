[:var_set('', """
# Compile command
aoikpdfbookmark -s README.src.md -n aoikpdfbookmark.ext.all::nto -g README.md
""")
]\
[:HDLR('heading', 'heading')]\
# AoikTinyHTTPdStudy
Add line-by-line comments to
[Tiny HTTPd](https://sourceforge.net/projects/tinyhttpd/).

Based on version 0.1.0.

Modified to compile in Linux:
```
gcc -W -Wall -o httpd httpd.c
```

Original license applies.
