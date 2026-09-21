/*
 * Arm SCP/MCP Software
 * Copyright (c) 2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MOD_GPIO_H
#define MOD_GPIO_H

#include <fwk_id.h>

#include <stdbool.h>

/*!
 * \addtogroup GroupModules Modules
 * \{
 */

/*!
 * \defgroup GroupGPIO GPIO HAL
 * \brief Synchronous GPIO access with one element per pin.
 * \{
 */

/*! GPIO element configuration. */
struct mod_gpio_dev_config {
    /*! Identifier of the pin in the driver module. */
    fwk_id_t driver_id;

    /*! Identifier of the GPIO driver API. */
    fwk_id_t driver_api_id;
};

/*! GPIO HAL API indices. */
enum mod_gpio_api_idx {
    MOD_GPIO_API_IDX_GPIO,
    MOD_GPIO_API_IDX_COUNT,
};

/*!
 * \brief GPIO client API.
 *
 * \details All operations are synchronous and return standard framework
 *     status codes. A pin is identified by its GPIO HAL element ID. An
 *     unsupported operation returns FWK_E_SUPPORT. Levels are physical pin
 *     levels; no active-low inversion is performed by the HAL.
 */
struct mod_gpio_api {
    /*!
     * \brief Select output (true) or input (false) direction.
     * \param id GPIO HAL element ID.
     * \param output Whether to enable the pin's output driver.
     */
    int (*set_direction)(fwk_id_t id, bool output);

    /*!
     * \brief Set the output latch high (true) or low (false).
     * \param id GPIO HAL element ID.
     * \param value Output level. Does not change the pin direction.
     */
    int (*write)(fwk_id_t id, bool value);

    /*!
     * \brief Sample the pin level.
     * \param id GPIO HAL element ID.
     * \param[out] value High (true) or low (false), valid on success only.
     * \retval FWK_E_PARAM The value pointer is NULL.
     */
    int (*read)(fwk_id_t id, bool *value);
};

/*!
 * \brief GPIO driver API.
 * \details Same synchronous semantics as mod_gpio_api, but IDs identify
 *     driver pins. Each callback may be NULL for an unsupported operation.
 */
struct mod_gpio_driver_api {
    /*! Select output (true) or input (false) direction. */
    int (*set_direction)(fwk_id_t id, bool output);

    /*! Set the output latch without changing direction. */
    int (*write)(fwk_id_t id, bool value);

    /*! Read the physical pin level, valid on success only. */
    int (*read)(fwk_id_t id, bool *value);
};

/*! \} */
/*! \} */

#endif /* MOD_GPIO_H */
