/* SPDX-License-Identifier: BSD-3-Clause */
/* QVP fixture only: all pins are disconnected except GPIO1->2 and GPIO9->10. */
#include <mod_gpio.h>
#include <mod_i2c.h>
#include <mod_pmic.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <string.h>

static const struct mod_gpio_api *gpio;
static const struct mod_i2c_api *i2c;
static const struct mod_pmic_api *pmic;

static int transfer(uint8_t reg, uint8_t *data, uint8_t size, bool read)
{
    uint8_t tx[12];
    struct mod_i2c_request request = {
        .target_address = 0x48,
        .transmit_data = tx,
        .transmit_byte_count = read ? 1 : size + 1,
        .receive_data = read ? data : NULL,
        .receive_byte_count = read ? size : 0,
    };

    tx[0] = reg;
    if (!read)
        memcpy(tx + 1, data, size);
    return i2c->transfer_as_controller(
        FWK_ID_ELEMENT(FWK_MODULE_IDX_I2C, 0), &request);
}

static int bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round)
        return FWK_SUCCESS;
    status = fwk_module_bind(FWK_ID_MODULE(FWK_MODULE_IDX_GPIO),
        FWK_ID_API(FWK_MODULE_IDX_GPIO, MOD_GPIO_API_IDX_GPIO), &gpio);
    if (status != FWK_SUCCESS)
        return status;
    status = fwk_module_bind(FWK_ID_ELEMENT(FWK_MODULE_IDX_I2C, 0),
        mod_i2c_api_id_i2c, &i2c);
    if (status != FWK_SUCCESS)
        return status;
    return fwk_module_bind(FWK_ID_ELEMENT(FWK_MODULE_IDX_PMIC, 0),
        FWK_ID_API(FWK_MODULE_IDX_PMIC, MOD_PMIC_API_IDX_PMIC), &pmic);
}

/* QVP programming-model test only: these writes do not qualify analog rails. */
static int check_pmic(void)
{
    const fwk_id_t id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_PMIC, 0);
    uint32_t original_uv[9], actual_uv, test_uv;
    bool original_enabled[9], actual_enabled;
    unsigned int rail, checks = 0;
    int status, result, rail_restore, restore_status = FWK_SUCCESS;

    FWK_LOG_INFO("[TPS6594-PMIC-TEST] begin path=pmic->tps6594->i2c->dw_apb_i2c");
    /* Capture every rail before the first write, so any failure is reversible. */
    for (rail = 0; rail < 9; rail++) {
        status = pmic->get_voltage(id, rail, &original_uv[rail]);
        if (status != FWK_SUCCESS)
            goto snapshot_failed;
        status = pmic->get_enabled(id, rail, &original_enabled[rail]);
        if (status != FWK_SUCCESS)
            goto snapshot_failed;
    }

    for (rail = 0; rail < 9; rail++) {
        /* Both voltages are representable by all BUCK and LDO selectors. */
        test_uv = original_uv[rail] == 1200000 ? 1300000 : 1200000;
        status = pmic->set_voltage(id, rail, test_uv);
        if (status != FWK_SUCCESS)
            goto restore;
        status = pmic->get_voltage(id, rail, &actual_uv);
        if (status != FWK_SUCCESS)
            goto restore;
        if (actual_uv != test_uv) {
            status = FWK_E_DEVICE;
            goto restore;
        }
        checks++;
        status = pmic->set_enabled(id, rail, !original_enabled[rail]);
        if (status != FWK_SUCCESS)
            goto restore;
        status = pmic->get_enabled(id, rail, &actual_enabled);
        if (status != FWK_SUCCESS)
            goto restore;
        if (actual_enabled == original_enabled[rail]) {
            status = FWK_E_DEVICE;
            goto restore;
        }
        checks++;
        FWK_LOG_INFO("[TPS6594-PMIC-TEST] rail=%u original_uv=%u test_uv=%u original_enable=%u voltage=PASS enable=PASS",
            rail, original_uv[rail], test_uv, original_enabled[rail]);
    }

restore:
    if (status != FWK_SUCCESS)
        FWK_LOG_ERR("[TPS6594-PMIC-TEST] rail=%u status=%d", rail, status);
    /* Attempt both writes and both readbacks for every rail despite errors. */
    for (rail = 0; rail < 9; rail++) {
        rail_restore = FWK_SUCCESS;
        result = pmic->set_voltage(id, rail, original_uv[rail]);
        if (result != FWK_SUCCESS)
            rail_restore = result;
        result = pmic->set_enabled(id, rail, original_enabled[rail]);
        if (result != FWK_SUCCESS)
            rail_restore = result;
        result = pmic->get_voltage(id, rail, &actual_uv);
        if (result != FWK_SUCCESS || actual_uv != original_uv[rail])
            rail_restore = result == FWK_SUCCESS ? FWK_E_DEVICE : result;
        result = pmic->get_enabled(id, rail, &actual_enabled);
        if (result != FWK_SUCCESS || actual_enabled != original_enabled[rail])
            rail_restore = result == FWK_SUCCESS ? FWK_E_DEVICE : result;
        if (rail_restore != FWK_SUCCESS)
            restore_status = rail_restore;
        FWK_LOG_INFO("[TPS6594-PMIC-TEST] rail=%u restore=%s status=%d",
            rail, rail_restore == FWK_SUCCESS ? "PASS" : "FAIL", rail_restore);
    }
    FWK_LOG_INFO("[TPS6594-PMIC-TEST] restore=%s rails=9 status=%d",
        restore_status == FWK_SUCCESS ? "PASS" : "FAIL", restore_status);
    if (status == FWK_SUCCESS)
        status = restore_status;
    if (status == FWK_SUCCESS && checks != 18)
        status = FWK_E_DEVICE;
    FWK_LOG_INFO("[TPS6594-PMIC-TEST] final=%s checks=%u status=%d",
        status == FWK_SUCCESS ? "PASS" : "FAIL", checks, status);
    return status;

snapshot_failed:
    FWK_LOG_ERR("[TPS6594-PMIC-TEST] snapshot rail=%u status=%d no writes performed",
        rail, status);
    FWK_LOG_INFO("[TPS6594-PMIC-TEST] final=FAIL checks=0 status=%d", status);
    return status;
}

static int check_pin(unsigned int pin, bool expected, unsigned int *checks)
{
    bool actual;
    int status = gpio->read(FWK_ID_ELEMENT(FWK_MODULE_IDX_GPIO, pin), &actual);

    if (status != FWK_SUCCESS)
        return status;
    if (actual != expected) {
        FWK_LOG_ERR("[TPS6594-HAL-TEST] pin=%u expected=%u actual=%u",
            pin, expected, actual);
        return FWK_E_DEVICE;
    }
    (*checks)++;
    return FWK_SUCCESS;
}

static int start(fwk_id_t id)
{
    uint8_t original[11], outputs[2], interrupts[2];
    uint8_t input_conf[11], drive_conf[11], verify[11], new_interrupts[2];
    unsigned int pin, level, other, pair, checks = 0;
    int status, restore_status = FWK_SUCCESS, result;
    fwk_id_t pin_id;

    FWK_LOG_INFO("[TPS6594-HAL-TEST] begin path=gpio->tps6594->i2c->dw_apb_i2c");
    status = transfer(0x31, original, 11, true);
    if (status != FWK_SUCCESS)
        return status;
    status = transfer(0x3d, outputs, 2, true);
    if (status != FWK_SUCCESS)
        return status;
    status = transfer(0x63, interrupts, 2, true);
    if (status != FWK_SUCCESS)
        return status;
    for (pin = 0; pin < 11; pin++) {
        input_conf[pin] = original[pin] & ~0xe1U;
        drive_conf[pin] = input_conf[pin] & ~2U; /* push-pull, initially input */
    }
    status = transfer(0x31, drive_conf, 11, false);
    if (status != FWK_SUCCESS)
        goto restore;
    for (pin = 0; pin < 11; pin++) {
        pin_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_GPIO, pin);
        status = gpio->write(pin_id, false);
        if (status != FWK_SUCCESS)
            goto restore;
        status = gpio->set_direction(pin_id, true);
        if (status != FWK_SUCCESS)
            goto restore;
    }
    /* Walking-one and all-low validate every output and bank isolation. */
    for (pin = 0; pin < 11; pin++) {
        pin_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_GPIO, pin);
        for (level = 0; level < 2; level++) {
            status = gpio->write(pin_id, level == 0);
            if (status != FWK_SUCCESS)
                goto restore;
            for (other = 0; other < 11; other++) {
                status = check_pin(other, other == pin && level == 0, &checks);
                if (status != FWK_SUCCESS)
                    goto restore;
            }
        }
        FWK_LOG_INFO("[TPS6594-HAL-TEST] output pin=%u high=PASS low=PASS isolation=PASS", pin);
    }
    for (pair = 0; pair < 2; pair++) {
        pin = pair * 8;
        status = gpio->set_direction(FWK_ID_ELEMENT(FWK_MODULE_IDX_GPIO, pin + 1), false);
        if (status != FWK_SUCCESS)
            goto restore;
        for (level = 0; level < 4; level++) {
            status = gpio->write(FWK_ID_ELEMENT(FWK_MODULE_IDX_GPIO, pin), level & 1);
            if (status != FWK_SUCCESS)
                goto restore;
            status = check_pin(pin + 1, level & 1, &checks);
            if (status != FWK_SUCCESS)
                goto restore;
        }
        FWK_LOG_INFO("[TPS6594-HAL-TEST] loop out=%u in=%u levels=0101 status=PASS", pin, pin + 1);
    }
restore:
    /* Disable outputs before restoring latches, then restore mux/drive/direction. */
    result = transfer(0x31, input_conf, 11, false);
    if (result != FWK_SUCCESS)
        restore_status = result;
    result = transfer(0x3d, outputs, 2, false);
    if (result != FWK_SUCCESS)
        restore_status = result;
    result = transfer(0x31, original, 11, false);
    if (result != FWK_SUCCESS)
        restore_status = result;
    result = transfer(0x31, verify, 11, true);
    if (result != FWK_SUCCESS || memcmp(verify, original, 11))
        restore_status = result == FWK_SUCCESS ? FWK_E_DEVICE : result;
    result = transfer(0x3d, verify, 2, true);
    if (result != FWK_SUCCESS || memcmp(verify, outputs, 2))
        restore_status = result == FWK_SUCCESS ? FWK_E_DEVICE : result;
    /* Clear only newly generated GPIO leaves, preserving preexisting events. */
    result = transfer(0x63, new_interrupts, 2, true);
    if (result == FWK_SUCCESS) {
        new_interrupts[0] &= ~interrupts[0] & 7;
        new_interrupts[1] &= ~interrupts[1];
        result = transfer(0x63, new_interrupts, 2, false);
    }
    if (result != FWK_SUCCESS)
        restore_status = result;
    result = transfer(0x63, verify, 2, true);
    if (result != FWK_SUCCESS || (verify[0] & 7) != (interrupts[0] & 7) ||
        verify[1] != interrupts[1])
        restore_status = result == FWK_SUCCESS ? FWK_E_DEVICE : result;
    FWK_LOG_INFO("[TPS6594-HAL-TEST] restore=%s checks=%u status=%d",
        restore_status == FWK_SUCCESS ? "PASS" : "FAIL", checks, restore_status);
    if (status == FWK_SUCCESS)
        status = restore_status;
    if (status == FWK_SUCCESS && checks != 250)
        status = FWK_E_DEVICE;
    FWK_LOG_INFO("[TPS6594-HAL-TEST] final=%s checks=%u status=%d",
        status == FWK_SUCCESS && checks == 250 ? "PASS" : "FAIL", checks, status);
    if (status == FWK_SUCCESS)
        status = check_pmic();
    return status;
}

static int init(fwk_id_t id, unsigned int count, const void *data)
{
    return FWK_SUCCESS;
}

const struct fwk_module module_test_tps6594 = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .init = init,
    .bind = bind,
    .start = start,
};

const struct fwk_module_config config_test_tps6594 = { 0 };
