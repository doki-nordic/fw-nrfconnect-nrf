.. _wireshark_rtt_hci_plugin:

HCI over RTT plugin for Wireshark
#################################

.. contents::
   :local:
   :depth: 2

The `Wireshark <https://www.wireshark.org/>`_ is a popular application for network traffic capturing and analysis.
It also support Bluetooth HCI protocol analysis.

The script contained in the :file:`nrf/scripts/wireshark/rtt_hci_plugin.py` file is a plugin for Wireshark that allows
you capturing HCI packets directly from device over Segger J-Link RTT.

Installation and configuration
******************************

#. Run the script with ``--install`` option to install the plugin.

   .. parsed-literal::
      :class: highlight

      python rtt_hci_plugin.py --install

   If automatic installation failed, see :ref:`wireshark_rtt_hci_plugin Manual installation`.

#. On target device, enable :kconfig:option:`CONFIG_BT_DEBUG_MONITOR_RTT`, rebuild, and flash your application.
   For more details, see :ref:`wireshark_rtt_hci_plugin Kconfig options`.

#. In the Wireshark, click little gear icon near the interface name ``Bluetooth HCI monitor packets over RTT: bt_hci_rtt``
   to configure J-Link RTT settings and start capture process.
   For more details, see :ref:`wireshark_rtt_hci_plugin J-Link RTT settings window`.

.. _wireshark_rtt_hci_plugin Kconfig options:

Kconfig options
***************

The :kconfig:option:`CONFIG_BT_DEBUG_MONITOR_RTT` option is required to capture HCI packets with this plugin.
You can also change related options:

* :kconfig:option:`CONFIG_BT_DEBUG_MONITOR_RTT_BUFFER_SIZE` contains the RTT buffer size.
  Increase it to reduce probability of dropping packets.
* :kconfig:option:`CONFIG_BT_DEBUG_MONITOR_RTT_BUFFER` contains the RTT buffer number which is ``1`` by default.
  Set it if the default buffer number is already taken.
* TODO: more configs related to printk and logging over packet capture.

.. _wireshark_rtt_hci_plugin J-Link RTT settings window:

J-Link RTT settings window
**************************

The J-Link RTT settings window allows you to configure standard RTT settings in ``Main`` and ``Optional`` tabs.

* ``Device`` field  must match your device connected to J-Link.
  Click ``Help`` button to see the list of all available devices.
* ``RTT Channel`` field must match :kconfig:option:`CONFIG_BT_DEBUG_MONITOR_RTT_BUFFER`.
  They both are ``1`` by default, so in most cases you don't need to change them.
* If you don't have :file:`JLinkRTTLogger` executable on your ``PATH`` environment variable,
  you have to configure it manually in the ``JLinkRTTLogger Executable`` field.
* If you have more J-Links connected, you can fill ``Serial Number`` field.
  Otherwise, you will be prompted to select a device at capture startup.

The ``Debug`` tab allows you to write debug logs to the files.

If you have ``Save parameter(s) on capture start`` checked,
the configuration will be saved and reused next time you start the capture on this interface.

.. _wireshark_rtt_hci_plugin Manual installation:

Manual installation
*******************

If automatic installation failed, you can install plugin manually.

#. Copy :file:`nrf/scripts/wireshark/rtt_hci_plugin.py` file to the Wireshark extcap directory.
   To locate the directory, in the Wireshark, go to ``Help`` → ``About Wireshark`` → ``Folders`` → ``Personal Extcap path``.
   If you want to install it globally for all users, use the ``Global Extcap path``.

#. Prepare environment for the script.

   .. tabs::

      .. group-tab:: Windows
         
         Run the script manually after coping it to the plugins directory:

         .. parsed-literal::
            :class: highlight

            python rtt_hci_plugin.py

         It will configure an environment and download all necessary Python modules.

      .. group-tab:: Linux

         This step in not needed in Linux.

      .. group-tab:: macOS

         This step in not needed in macOS.

#. After restarting Wireshark, you should see ``Bluetooth HCI monitor packets over RTT: bt_hci_rtt`` on the interface list.
