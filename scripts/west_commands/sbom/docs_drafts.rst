
Requirements
############

The SBOM Script requires additional Python packages to be installed.

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

..

.. note::
    The script uses the ScanCode-Toolkit which requires additional dependencies to be installed on Linux system.
    To install the required tools on Ubuntu run::

      sudo apt install python-dev bzip2 xz-utils zlib1g libxml2-dev libxslt1-dev libpopt0

    For more details see https://scancode-toolkit.readthedocs.io/en/latest/getting-started/install.html


Specifying input:

* Application BOM generated from build directory:
  -d build_directory

* List of files:
  --input-files file1 file2 ...
  Each argument of this option can contain globs as defined by:
  https://docs.python.org/3/library/pathlib.html#pathlib.Path.glob
  For example, if you want to include all ``.c`` files from current directory
  and all subdirectories recursively:
  --input-files '**/*.c'
  Remember to put correct quotes around globs, to make sure that the glob will
  not be resolved by the shell, but it will go untouched to the script.
  You can prefix pattern with the exclamation mark ``!`` to exclude some files.
  Patterns are evaluated from left to right, so ``!`` will exclude files from
  patterns before it, but not after. For example, if you want to include all
  ``.c`` files from current directory and all subdirectories recursively, except
  all ``main.c`` files.
  --input-files '**/*.c' '!**/main.c'

* File that contains list of files:
  --input-list-file list_file
  It does the same as ``--input-files``, but reads files and patterns from
  a file (one file or pattern per line). Files and patterns contained in the
  list file are relative to the list file location (not current directory).
  Comments starting with ``#`` are allowed.

Each of the above input options can be specified multiple times to provide
more input for the report generation, e.g. produce report for two applications.
They can be also mixed, e.g. produce report for the application and some
directory.
