# Kconfig documentation build configuration file

from pathlib import Path
import sys


# Paths ------------------------------------------------------------------------

NRF_BASE = Path(__file__).absolute().parents[2]

sys.path.insert(0, str(NRF_BASE / "doc" / "_utils"))
import utils

# General configuration --------------------------------------------------------

project = "Kconfig reference"
copyright = "2019-2021, Nordic Semiconductor"
author = "Nordic Semiconductor"
# NOTE: use blank space as version to preserve space
version = "&nbsp;"

sys.path.insert(0, str(NRF_BASE / "doc" / "_extensions"))

extensions = ["ncs_cache"]

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NRF_BASE / "doc" / "_static")]
html_title = project
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

html_theme_options = {
    "docsets": utils.get_docsets("kconfig"),
    "docsets_before": utils.get_docsets_before("kconfig"),
    "docsets_after": utils.get_docsets_after("kconfig"),
    "docset_current": utils.get_docset("kconfig")
}
# Options for ncs_cache --------------------------------------------------------

ncs_cache_docset = "kconfig"
ncs_cache_build_dir = utils.get_builddir()
ncs_cache_config = NRF_BASE / "doc" / "cache.yml"
ncs_cache_manifest = NRF_BASE / "west.yml"


def setup(app):
    app.add_css_file("css/common.css")
    app.add_css_file("css/kconfig.css")
