\ingroup GroupModules Modules
\addtogroup GroupGICx00MultiView Arm GICx00 Multi View

# Arm GICx00 multi view configuration module

The Arm GIC "multi view" feature allows firmware to allocate GIC resources into
up to three views. This allows systems to allocate the GIC to up to three
different OS or hypervisors that are running independent software stacks.

The gicx00_multiview module configures one or more multiview GICs by assigning
individual redistributors to views then assigning individual interrupts to
views.

Note that this module may be used with or without the gicx00 module. If
SCP-firmware is using one of the configured multiview views then the
gicx00_multiview module must be listed prior to gicx00 in SCP_MODULES so
that it is initialized first.

## Configuration

Each module element is mapped to an independent multiview-enabled GIC. Each
element requires:

 * A mapping of redistributor addresses to view numbers.
 * A mapping of interrupt IDs to view numbers.

An enumeration is provided for the view numbers.

An example configuration is shown below:

```
    static const struct mod_gicx00_multiview_redistributor_map
        redistributor_map[] = {
            { GICR_BASE_VIEW0_0, MOD_GICX00_MULTIVIEW_VIEW_1 },
            { GICR_BASE_VIEW0_1, MOD_GICX00_MULTIVIEW_VIEW_2 },
            { GICR_BASE_VIEW0_2, MOD_GICX00_MULTIVIEW_VIEW_3 },
        };

    static const struct mod_gicx00_multiview_spi_map spi_map[] = {
        { 32, MOD_GICX00_MULTIVIEW_VIEW_1 },
        { 33, MOD_GICX00_MULTIVIEW_VIEW_1 },

        { 34, MOD_GICX00_MULTIVIEW_VIEW_2 },
        { 37, MOD_GICX00_MULTIVIEW_VIEW_2 },

        { 40, MOD_GICX00_MULTIVIEW_VIEW_3 },
        { 41, MOD_GICX00_MULTIVIEW_VIEW_3 },
    };

    const struct fwk_element config_gicx00_multiview_ut[] = {
        { .name = "multiview-gic",
        .data = &((struct mod_gicx00_multiview_config){
            .gicd_base = GICD_BASE,
            .redistributor_map = redistributor_map,
            .redistributor_map_count = FWK_ARRAY_SIZE(redistributor_map),
            .spi_map = spi_map,
            .spi_map_count = FWK_ARRAY_SIZE(spi_map),
        }) },
        { 0 }
    };
```
