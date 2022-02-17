.. _sbom_script:

Software Bill of Materials
##########################

.. contents::
   :local:
   :depth: 2

The Software bill of materials is a Python script that provide a list of used licenses for a build application or for specific files.
The script can be used with the west tool.

.. note::
    The Software bill of materials script is experimental.
    The accuracy of detection is constantly verified.
    Both implementation and usage may change in the future.

Overview
********

The Software bill of materials script uses different types of detectors, depending on the configuration.
For a description of the detectors, see :ref:`Detectors` section.
The choice of detector will affect the detection speed and may also affect the detection coverage.

It is possible to create a list of licenses both for the built application and for the specified directory.

Requirements
************

The script requires additional Python packages to be installed.

Use the following commands to install the requirements for each repository.

.. tabs::

   .. group-tab:: Windows

      Enter the following command in a command-line window in the :file:`ncs` folder:

        .. parsed-literal::
           :class: highlight

           pip3 install -r nrf/scripts/requirements-west-sbom.txt

   .. group-tab:: Linux

      Enter the following command in a terminal window in the :file:`ncs` folder:

        .. parsed-literal::
           :class: highlight

           pip3 install --user -r nrf/scripts/requirements-west-sbom.txt

   .. group-tab:: macOS

      Enter the following command in a terminal window in the :file:`ncs` folder:

        .. parsed-literal::
           :class: highlight

           pip3 install -r nrf/scripts/requirements-west-sbom.txt

.. note::
    The script uses the ``ScanCode Toolkit`` which requires additional dependencies to be installed on Linux system.
    To install the required tools on Ubuntu run::

      sudo apt install python-dev bzip2 xz-utils zlib1g libxml2-dev libxslt1-dev libpopt0

    For more details see https://scancode-toolkit.readthedocs.io/en/latest/getting-started/install.html


Using the script
****************

See the script's help by running the following command::

  west sbom -h

Analysis of built application and report generation as html file::

  west sbom -d <build-directory> --license-detectors <detector/s> --output-html <file-name.html>

Analysis of selected files and report generation as html file::

  west sbom --input-files <file1 file2> --license-detectors <detector/s> --output-html <file-name.html>

Or by using the list of file::

  west sbom --input-list-file <list-file.txt> --license-detectors <detector/s> --output-html <file-name.html>

.. note::
    You can use globs (?, *, **) to provide more files. For example::

      west sbom --input-files src/**/* --license-detectors <detector/s> --output-html <file-name.html>

    See the :ref:`specifying_input` section for more details.

.. _Detectors:

Detectors
*********

List of implemented detectors:

* Detection based od spdx tags::

  --license-detectors spdx-tag

  Search for the SPDX-License-Identifier in the source code or the binary file.
  For guidelines, see: https://spdx.github.io/spdx-spec/using-SPDX-short-identifiers-in-source-files

* Full text detector::

  --license-detectors full-text

  Compare the contents of the license with the references that are stored in the database.

* ScanCode Toolkit::

  --license-detectors scancode-toolkit

  License detection by scancode-toolkit.
  For more details see: https://scancode-toolkit.readthedocs.io/en/stable/

* Cache database::

  --license-detectors cache-database --input-cache-database <cache-file.json>

  License detection is based on a predefined database.
  The license type is obtained from the database.

  .. note::
    You can generate the database base on e.g scancode-toolkit detector by running following command::

      west sbom --input-files <files ..> --license-detectors scancode-toolkit --output-cache-database <file-name.json>

.. _specifying_input:

Specifying input
****************

* Application BOM generated from build directory::

    -d build_directory

* List of files::

  --input-files file1 file2 ...

  Each argument of this option can contain globs as defined by:
  https://docs.python.org/3/library/pathlib.html#pathlib.Path.glob

  For example, if you want to include all ``.c`` files from current directory
  and all subdirectories recursively::

  --input-files '**/*.c'

  Remember to put correct quotes around globs, to make sure that the glob will
  not be resolved by the shell, but it will go untouched to the script.

  You can prefix pattern with the exclamation mark ``!`` to exclude some files.
  Patterns are evaluated from left to right, so ``!`` will exclude files from
  patterns before it, but not after. For example, if you want to include all
  ``.c`` files from current directory and all subdirectories recursively, except
  all ``main.c`` files.:

  --input-files '**/*.c' '!**/main.c'

* File that contains list of files::

  --input-list-file list_file

  It does the same as ``--input-files``, but reads files and patterns from
  a file (one file or pattern per line). Files and patterns contained in the
  list file are relative to the list file location (not current directory).
  Comments starting with ``#`` are allowed.

Each of the above input options can be specified multiple times to provide
more input for the report generation, e.g. produce report for two applications.
They can be also mixed, e.g. produce report for the application and some
directory.

Specifying output
*****************

* HTML report::

  --output-html <file-name.html>

  Generate output HTML report.

* Cache database::

  --output-cache-database <file.json>

  Generate output json cache database.
  The file can be used as reference database for the ``cache-database`` detector and also for custom purposes.
