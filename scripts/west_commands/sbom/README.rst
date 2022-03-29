.. _west_sbom:

Software Bill of Materials
##########################

.. contents::
   :local:
   :depth: 2

The Software Bill of Materials (SBOM) is a :ref:`west <zephyr:west>` extension command that can be invoked by ``west sbom``.
It provides a list of used licenses for an application build or the specific files.

.. note::
    Generating list of licenses from an application build is experimental.
    The accuracy of detection is constantly verified.
    Both implementation and usage may change in the future.


Overview
********

The process of using the ``sbom`` command involves the following steps:

#. Create list of input files based on provided command line arguments,
   for example, all source files used for building a specific application.
   For details, see :ref:`west_sbom Specifying input`.

#. Detect the license applied to each file,
   for example, read `SPDX identifier`_ from ``SPDX-License-Identifier`` tag.
   For details, see :ref:`west_sbom Detectors`.

#. Create output report containing all the files and license information related to them,
   for example, write a report file in HTML format.
   For details, see :ref:`west_sbom Specifying output`.


Requirements
************

The SBOM command requires additional Python packages to be installed.

Use the following command to install the requirements.

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
    The ``sbom`` command uses the `Scancode-Toolkit`_ that requires additional dependencies to be installed on a Linux system.
    To install the required tools on Ubuntu, run::

      sudo apt install python-dev bzip2 xz-utils zlib1g libxml2-dev libxslt1-dev libpopt0

    For more details, see `Scancode-Toolkit Installation`_.


Using the command
*****************

The following examples demonstrate the basic usage of the ``sbom`` command.

* To see the help, run the following command:

  .. code-block:: bash

    west sbom -h

* To get an analysis of the built application and generate a report to the ``sbom_report.html`` file in the build directory, run:

  .. parsed-literal::
     :class: highlight

      west sbom -d *build-directory*

* To analyze the selected files and generate a report to an HTML file, run:

  .. parsed-literal::
     :class: highlight

     west sbom --input-files *file1* *file2* --output-html *file-name.html*


.. _west_sbom Specifying input:

Specifying input
================

You can specify all input options several times to provide more input for the report generation, for example, generate a report for two applications.
You can also mix them, for example, to generate a report for the application and some directory.


* To get an application SBOM from a build directory, use the following option:

  .. code-block:: bash

     -d build_directory

  You have to first build the ``build_directory`` with the ``west build`` command using ``Ninja`` as the underlying build tool (default).

  This option requires the GNU ``ar`` tool.
  If you do not have it on your ``PATH``, you can pass it with the ``--ar`` option, for example:

  .. code-block:: bash

     --ar ~/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-ar

  The command searches for the files used during the build of :file:`zephyr/zephyr.elf` target.
  It also requires the :file:`zephyr/zephyr.map` file created by the linker.

  .. note::
      All the files that are not dependencies of the :file:`zephyr/zephyr.elf` target are not taken as an input.
      If the :file:`.elf` file is modified after the linking, the modifications are not applied.

  If your build directory contains more than one output target or it has a different name,
  you can add targets after the ``build_directory``.
  If the :file:`.map` file and the associated file:`.elf` file have different names,
  you can provide the :file:`.map` file after the ``:`` sign following the target,
  for example:

  .. parsed-literal::
     :class: highlight

     -d build_directory *target1.elf* *target2.elf*:*file2.map*

  .. note::
      The ``-d`` option is experimental.

* You can provide a list of input files directly on the command line:

  .. parsed-literal::
     :class: highlight

     --input-files *file1* *file2* ...

  Each argument of this option can contain globs as defined by `Python's Path.glob`_ with two additions:
  exclamation mark ``!`` to exclude files and absolute paths.

  For example, if you want to include all :file:`.c` files from the current directory and all subdirectories recursively:

  .. code-block:: bash

     --input-files '**/*.c'

  Make sure to have correct quotes around globs, to not have the glob resolved by your shell, and go untouched to the command.

  You can prefix a pattern with the exclamation mark ``!`` to exclude some files.
  Patterns are evaluated from left to right, so ``!`` excludes files from patterns before it, but not after.
  For example, if you want to include all :file:`.c` files from the current directory and all subdirectories recursively except all :file:`main.c` files, run:

  .. code-block:: bash

     --input-files '**/*.c' '!**/main.c'

* You can read a list of input files from a file:

  .. parsed-literal::
     :class: highlight

     --input-list-file *list_file*

  It does the same as ``--input-files``, but it reads files and patterns from a file (one file or pattern per line).
  Files and patterns contained in the list file are relative to the list file location (not the current directory).
  Comments starting with a ``#`` character are allowed.


.. _west_sbom Specifying output:

Specifying output
=================

You can specify the format of the report output using the ``output`` argument.

* To generate a report in HTML format:

  .. parsed-literal::
     :class: highlight

     --output-html *file-name.html*

  If you use ``-d`` option, you do not need to specify the report format.
  The :file:`sbom_report.html` file is generated in your build directory
  (the first one if you specify more than one build directory).

* To generate a cache database:

  .. parsed-literal::
     :class: highlight

     --output-cache-database *cache-database.json*

  For details, see ``cache-database`` detector.


.. _west_sbom Detectors:

Detectors
=========

The ``sbom`` command has the following detectors implemented:

* ``spdx-tag`` - search for the ``SPDX-License-Identifier`` in the source code or the binary file.
  For guidelines, see `SPDX identifier`_. Enabled by default.

* ``full-text`` - compare the contents of the source file with a small database of reference texts.
  The database is part of the ``sbom`` command. Enabled by default.

* ``scancode-toolkit`` - license detection by the `Scancode-Toolkit`_. Enabled and optional by default.

  If the ``scancode`` command is not on your ``PATH``, you can use the ``--scancode`` option to provide it, for example:

  .. code-block:: bash

     --scancode ~/scancode-toolkit/scancode

  This detector is optional because is significantly slower than the others.

* ``cache-database`` - use license information detected and cached earlier in the cache database file.
  Disabled by default.

  You have to provide the cache database file using the following argument:

  .. parsed-literal::
     :class: highlight

     --input-cache-database *cache-database.json*

  Each database entry has a path relative to the west workspace directory, a hash, and a list of detected licenses.
  If the file under detection has the same path and hash, the list of licenses from the database is used.

  .. note::
     To generate the database based on, for example the scancode-toolkit detector, run the following command:

     .. parsed-literal::
        :class: highlight

        west sbom --input-files *files ...* --license-detectors scancode-toolkit --output-cache-database *cache-database.json*

If you prefer a non-default set of detectors, you can provide a list of comma-separated detectors with the ``--license-detectors`` option, for example:

  .. code-block:: bash

     --license-detectors spdx-tag,scancode-toolkit

Some of the detectors are optional, which means that they are not executed for a file that
already has licenses detected by some other previously executed detector.
Detectors are executed from left to right using a list provided by the ``--license-detectors``.

  .. code-block:: bash

     --optional-license-detectors scancode-toolkit

Some detectors may run in parallel on all available CPU cores, which speeds up the detection time.
Use ``-n`` option to limit number of parallel threads or processes.
