# -*- coding: utf-8 -*-
#
# Zephyr documentation build configuration file, created by
# sphinx-quickstart on Fri May  8 11:43:01 2015.
#
# This file is execfile()d with the current directory set to its
# containing dir.
#
# Note that not all possible configuration values are present in this
# autogenerated file.
#
# All configuration values have a default; values that are commented out
# serve to show the default.

import sys
import os
from subprocess import CalledProcessError, check_output, DEVNULL

if "ZEPHYR_BASE" not in os.environ:
    print("$ZEPHYR_BASE environment variable undefined.")
    sys.exit("$ZEPHYR_BASE environment variable undefined.")
ZEPHYR_BASE = os.path.abspath(os.environ["ZEPHYR_BASE"])

if "ZEPHYR_BUILD" not in os.environ:
    sys.exit("$ZEPHYR_BUILD environment variable undefined.")
ZEPHYR_BUILD = os.path.abspath(os.environ["ZEPHYR_BUILD"])

NRF_BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# Add the 'extensions' directory to sys.path, to enable finding Sphinx
# extensions within.
sys.path.insert(0, os.path.join(ZEPHYR_BASE, 'doc', 'extensions'))

# Add the directory which contains the runners package as well,
# for autodoc directives on runners.xyz.
sys.path.insert(0, os.path.join(ZEPHYR_BASE, 'scripts', 'west_commands'))

west_found = False

try:
    desc = check_output(['west', 'list', '-f{abspath}', 'west'],
			stderr=DEVNULL,
			cwd=os.path.dirname(__file__))
    west_path = desc.decode(sys.getdefaultencoding()).strip()
    # Add west, to be able to pull in its API docs.
    sys.path.append(os.path.join(west_path, 'src'))
    west_found = True
except FileNotFoundError as e:
    # west not installed
    pass
except CalledProcessError as e:
    # west not able to list itself
    pass

# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#needs_sphinx = '1.0'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'breathe', 'sphinx.ext.todo',
    'sphinx.ext.extlinks',
    'sphinx.ext.autodoc',
    'zephyr.application',
    'zephyr.html_redirects',
    'only.eager_only',
    'zephyr.link-roles'
]

# Add any paths that contain templates here, relative to this directory.
#templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The encoding of source files.
#source_encoding = 'utf-8-sig'

# The master toctree document.
master_doc = 'index'

# General information about the project.
project = u'Zephyr Project'
copyright = u'2015-2019 Zephyr Project members and individual contributors.'
author = u'The Zephyr Project'

# The following code tries to extract the information by reading the Makefile,
# when Sphinx is run directly (e.g. by Read the Docs).
try:
    version_major = None
    version_minor = None
    patchlevel = None
    extraversion = None
    for line in open(os.path.join(ZEPHYR_BASE, 'VERSION')):
        key, val = [x.strip() for x in line.split('=', 2)]
        if key == 'VERSION_MAJOR':
            version_major = val
        if key == 'VERSION_MINOR':
            version_minor = val
        elif key == 'PATCHLEVEL':
            patchlevel = val
        elif key == 'EXTRAVERSION':
            extraversion = val
        if version_major and version_minor and patchlevel and extraversion:
            break
except Exception:
    pass
finally:
    if version_major and version_minor and patchlevel and extraversion is not None :
        version = release = version_major + '.' + version_minor + '.' + patchlevel
        if extraversion != '':
            version = release = version + '-' + extraversion
    else:
        sys.stderr.write('Warning: Could not extract kernel version\n')
        version = release = "unknown version"
        version = release = os.getenv('KERNELVERSION','1.9.0')

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# There are two options for replacing |today|: either, you set today to some
# non-false value, then it is used:
#today = ''
# Else, today_fmt is used as the format for a strftime call.
#today_fmt = '%B %d, %Y'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['_build']
if not west_found:
    exclude_patterns.append('**/*west-apis*')
else:
    exclude_patterns.append('**/*west-not-found*')

# The reST default role (used for this markup: `text`) to use for all
# documents.
#default_role = None

# If true, '()' will be appended to :func: etc. cross-reference text.
#add_function_parentheses = True

# If true, the current module name will be prepended to all description
# unit titles (such as .. function::).
#add_module_names = True

# If true, sectionauthor and moduleauthor directives will be shown in the
# output. They are ignored by default.
#show_authors = False

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# Additional lexer for Pygments (syntax highlighting)
from lexer.DtsLexer import DtsLexer
from sphinx.highlighting import lexers
lexers['DTS'] = DtsLexer()

# A list of ignored prefixes for module index sorting.
#modindex_common_prefix = []

# If true, keep warnings as "system message" paragraphs in the built documents.
#keep_warnings = False

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = False

rst_epilog = """
.. include:: /substitutions.txt
"""

# -- Options for HTML output ----------------------------------------------

html_theme = "zephyr"
html_theme_path = ['{}/doc/themes'.format(NRF_BASE)]
html_favicon = '{}/doc/static/images/favicon.ico'.format(NRF_BASE)

if tags.has('release'):  # pylint: disable=undefined-variable
    is_release = True
    docs_title = 'Docs / %s' %(version)
else:
    is_release = False
    docs_title = 'Docs / Latest'

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
html_title = "Zephyr Project Documentation"

# This value determines the text for the permalink; it defaults to "¶".
# Set it to None or the empty string to disable permalinks.
#html_add_permalinks = ""

# A shorter title for the navigation bar.  Default is the same as html_title.
#html_short_title = None

# The name of an image file (relative to this directory) to place at the top
# of the sidebar.
#html_logo = None

# The name of an image file (within the static path) to use as favicon of the
# docs.  This file should be a Windows icon file (.ico) being 16x16 or 32x32
# pixels large.
#html_favicon = None

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['{}/doc/static'.format(NRF_BASE),
                    '{}/doc/static'.format(ZEPHYR_BASE)]

# Add any extra paths that contain custom files (such as robots.txt or
# .htaccess) here, relative to this directory. These files are copied
# directly to the root of the documentation.
#html_extra_path = []

# If not '', a 'Last updated on:' timestamp is inserted at every page bottom,
# using the given strftime format.
html_last_updated_fmt = '%b %d, %Y'

# If true, SmartyPants will be used to convert quotes and dashes to
# typographically correct entities.
#html_use_smartypants =

# Custom sidebar templates, maps document names to template names.
#html_sidebars = {}

# Additional templates that should be rendered to pages, maps page names to
# template names.
#html_additional_pages = {}

# If false, no module index is generated.
html_domain_indices = False

# If false, no index is generated.
html_use_index = True

# If true, the index is split into individual pages for each letter.
html_split_index = True

# If true, links to the reST sources are added to the pages.
#html_show_sourcelink =

# If true, "Created using Sphinx" is shown in the HTML footer. Default is True.
html_show_sphinx = False

# If true, "(C) Copyright ..." is shown in the HTML footer. Default is True.
html_show_copyright = True

# If true, license is shown in the HTML footer. Default is True.
html_show_license = True

# If true, an OpenSearch description file will be output, and all pages will
# contain a <link> tag referring to it.  The value of this option must be the
# base URL from which the finished HTML is served.
#html_use_opensearch = ''

# This is the file name suffix for HTML files (e.g. ".xhtml").
#html_file_suffix = None

# Language to be used for generating the HTML full-text search index.
# Sphinx supports the following languages:
#   'da', 'de', 'en', 'es', 'fi', 'fr', 'hu', 'it', 'ja'
#   'nl', 'no', 'pt', 'ro', 'ru', 'sv', 'tr'
#html_search_language = 'en'

sourcelink_suffix = '.txt'

# A dictionary with options for the search language support, empty by default.
# Now only 'ja' uses this config value
#html_search_options = {'type': 'default'}

# The name of a javascript file (relative to the configuration directory) that
# implements a search results scorer. If empty, the default will be used.
#html_search_scorer = 'scorer.js'


breathe_projects = {
    "Zephyr": "{}/doxygen/xml".format(ZEPHYR_BUILD),
    "doc-examples": "{}/doxygen/xml".format(ZEPHYR_BUILD)
}
breathe_default_project = "Zephyr"

# Qualifiers to a function are causing Sphihx/Breathe to warn about
# Error when parsing function declaration and more.  This is a list
# of strings that the parser additionally should accept as
# attributes.
cpp_id_attributes = ['__syscall', '__syscall_inline', '__deprecated',
    '__may_alias', '__used', '__unused', '__weak',
    '__DEPRECATED_MACRO', 'FUNC_NORETURN' ]

# docs_title is used in the breadcrumb title in the zephyr docs theme
html_context = {
    'show_license': html_show_license,
    'docs_title': docs_title,
    'is_release': is_release,
}

extlinks = {'jira': ('https://jira.zephyrproject.org/browse/%s', ''),
            'github': ('https://github.com/zephyrproject-rtos/zephyr/issues/%s', '')
           }

# some configuration for linkcheck builder
#   noticed that we're getting false-positive link errors on JIRA, I suspect
#   because it's taking too long for the server to respond so bump up the
#   timeout (default=5) and turn off anchor checks (so only a HEAD request is
#   done - much faster) Leave the ignore commented in case we want to remove
#   jira link checks later...

linkcheck_timeout = 30
linkcheck_workers = 10
# linkcheck_ignore = [r'https://jira\.zephyrproject\.org/']
linkcheck_anchors = False

def setup(app):
    app.add_stylesheet("zephyr-custom.css")
    app.add_stylesheet("css/zephyr.css")
    app.add_stylesheet("css/common.css")
