#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(SB_CONFIG_ML_APP_INCLUDE_REMOTE_IMAGE AND DEFINED SB_CONFIG_ML_APP_REMOTE_BOARD)
  # Add remote project
  ExternalZephyrProject_Add(
    APPLICATION remote
    SOURCE_DIR ${APP_DIR}/remote
    BOARD ${SB_CONFIG_ML_APP_REMOTE_BOARD}
  )

set_config_bool(machine_learning CONFIG_ML_APP_INCLUDE_REMOTE_IMAGE y)
endif()
