\ingroup GroupPLATFORMModule PLATFORM Product Modules
\addtogroup GroupSMCFClient Aspen SMCF Client

# RD-Aspen SMCF Client module

## Overview

The **SMCF Client** module orchestrates sampling and data retrieval for RD-Aspen
SMCF monitor groups (MGIs). It binds to the platform SMCF sampling API and the
AMU/Sensor SMCF driver APIs, and reacts to SMCF "new sample ready" notifications
to fetch and optionally print data. It also listens for power domain
transitions to start/stop sampling across all MGIs.

---

## Configuration

The module is configured per element (one element per SMCF MGI). Each element
defines the corresponding SMCF element ID and a list of MLIs with their monitor
type.

```c
struct mod_smcf_client_mgi_conf {
    fwk_id_t smcf_mgi_id;
    struct mod_smcf_client_mli_conf *mlis;
};

struct mod_smcf_client_mli_conf {
    enum mod_smcf_client_monitor_type_idx type;
};
```

Supported MLI types:

- `MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_AMU`
- `MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR`

Element count must match the SMCF module element count.

---

## Dependencies

The SMCF Client module binds to the following APIs:

| Module               | API Identifier                                      |
|----------------------|-----------------------------------------------------|
| `MOD_PLATFORM_SMCF`  | `MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API`           |
| `MOD_SMCF`           | `MOD_SMCF_API_IDX_CONTROL`                          |
| `MOD_AMU_SMCF_DRV`   | `MOD_AMU_SMCF_DRV_API_IDX_DATA`                     |
| `MOD_SENSOR_SMCF_DRV`| `MOD_SENSOR_SMCF_DRV_API_IDX_GET_MULTIPLE_SAMPLES`  |

When `BUILD_HAS_NOTIFICATION` is enabled, the module also subscribes to:

| Notification Source | Notification ID                                   |
|---------------------|----------------------------------------------------|
| `MOD_SMCF`           | `mod_smcf_notification_id_new_data_sample_ready`  |
| `MOD_POWER_DOMAIN`  | `MOD_PD_NOTIFICATION_IDX_POWER_STATE_TRANSITION`  |

---

## API

The module exposes a single control API:

```c
struct mod_smcf_client_control_api {
    int (*start_sampling_all_mgis)(void);
    int (*stop_sampling_all_mgis)(void);
    int (*toggle_print)(void);
};
```

- `start_sampling_all_mgis()`: Start sampling on all MGIs.
- `stop_sampling_all_mgis()`: Stop sampling on all MGIs.
- `toggle_print()`: Toggle printing of sampled values (used by tests).

---

## Notification Handling

With notifications enabled, the module:

- Starts or stops sampling on all MGIs when the system power domain transitions
  to ON or OFF/SLEEP states.
- Retrieves data for each MLI on `new_data_sample_ready` notifications.
  AMU MLIs use `MOD_AMU_SMCF_DRV` counters, and Sensor MLIs use
  `MOD_SENSOR_SMCF_DRV` samples.

---

## Function Summary

- `smcf_client_init()`: Validate element count and allocate context.
- `smcf_client_element_init()`: Store per-MGI configuration.
- `smcf_client_bind()`: Bind to platform sampling, SMCF control, and driver APIs.
- `smcf_client_process_bind_request()`: Provide control API to clients.
- `smcf_client_start()`: Subscribe to notifications when enabled.
- `smcf_client_process_notification()`: Handle sample-ready and power-domain
  events.

---

## Example Configuration

```c
static const struct fwk_element elements[] = {
    [0] = {
        .name = "SMD_MGI",
        .sub_element_count = 1,
        .data = &((struct mod_smcf_client_mgi_conf) {
            .smcf_mgi_id = FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0),
            .mlis = (struct mod_smcf_client_mli_conf[]) {
                [0] = { .type = MOD_SMCF_CLIENT_MONITOR_TYPE_IDX_SENSOR },
            },
        }),
    },
    [1] = { 0 },
};

struct fwk_module_config config_smcf_client = {
    .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(elements),
};
```

---

## Example Usage

```c
const struct mod_smcf_client_control_api *ctrl_api;

fwk_module_bind(
    FWK_ID_MODULE(FWK_MODULE_IDX_SMCF_CLIENT),
    FWK_ID_API(FWK_MODULE_IDX_SMCF_CLIENT, MOD_SMCF_CLIENT_API_IDX_CONTROL),
    &ctrl_api);

ctrl_api->start_sampling_all_mgis();
```
