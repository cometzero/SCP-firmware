\ingroup GroupPLATFORMModule PLATFORM Product Modules
\addtogroup GroupPlatformSMCF Aspen Platform SMCF

# RD-Aspen Platform SMCF module

## Overview

The **Platform SMCF** module provides a platform-specific wrapper around the
generic SMCF driver. It binds to the SMCF data API and exposes a sampling API
for RD-Aspen clients. The wrapper forwards start/stop sampling calls to SMCF,
and provides a hook to add external trigger signaling if needed.

---

## Configuration

No module-level or element-level configuration is required.

Clients pass SMCF element IDs (MGI instances) when starting or stopping
sampling.

---

## Dependencies

The Platform SMCF module binds to the following API:

| Module      | API Identifier          |
|-------------|-------------------------|
| `MOD_SMCF`  | `MOD_SMCF_API_IDX_DATA` |

---

## API

The module exposes a single API index:

- `MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API`

This API is a subset of `struct smcf_data_api` and supports:

```c
struct smcf_data_api {
    int (*start_data_sampling)(fwk_id_t element_id);
    int (*stop_data_sampling)(fwk_id_t element_id);
};
```

`get_data()` is not provided by this wrapper. Clients that need sampled data
should bind directly to the SMCF module data API.

---

## Function Summary

- `platform_smcf_mod_init()`: Initialize the module context.
- `platform_smcf_bind()`: Bind to the SMCF data API.
- `platform_smcf_process_bind_request()`: Provide the sampling API to clients.
- `smcf_start_sampling()`: Forward start sampling requests to SMCF.
- `smcf_stop_sampling()`: Forward stop sampling requests to SMCF.

---

## Example Usage

```c
const struct smcf_data_api *sampling_api;

fwk_module_bind(
    FWK_ID_MODULE(FWK_MODULE_IDX_PLATFORM_SMCF),
    FWK_ID_API(
        FWK_MODULE_IDX_PLATFORM_SMCF,
        MOD_SMCF_PLATFORM_API_IDX_SAMPLING_API),
    &sampling_api);

sampling_api->start_data_sampling(FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0));

sampling_api->stop_data_sampling(FWK_ID_ELEMENT(FWK_MODULE_IDX_SMCF, 0));
```
