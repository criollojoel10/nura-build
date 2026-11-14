# nura-build

Compilaciones para **Nura en el Xiaomi Redmi Note 12 Pro 5G (ruby, MediaTek
MT6877)**, en un repo publico para que GitHub Actions sea gratuito (el privado
agoto su presupuesto) y para poder usar runners **ARM nativos** (sin qemu).

No contiene nada del telefono: ni backups, ni claves, ni logs, ni imagenes de
particiones. Solo el port de postmarketOS, el parche de Mesa y los workflows.

## Workflows

| workflow | que hace | salida |
| --- | --- | --- |
| `mesa.yml` | compila Mesa 26.2.6 para aarch64 con la fila de **Mali-G68** en panfrost (backport del commit upstream `93d2c51f`, MR !43414) | `.apk` en el release indicado |
| `rootfs.yml` | construye el rootfs postmarketOS con `pmbootstrap` (`ui` seleccionable, por defecto `plasma-mobile`) y lo verifica | `.img.zst` + `SHA256SUMS` |

### Mesa

El unico cambio real es una fila en `src/panfrost/model/pan_model.c`:

```c
VALHALL_MODEL(PAN_PROD_ID(9, 2, 4), 0, "G68", "G78", MODEL_ANISO(ALL),
              MODEL_TB_SIZES(16384, 8192), MODEL_RATES_X(2, 4, 8, 32, 32, 8)),
```

Sin ella `pan_get_model()` no encuentra el chip (0x9204) y Mesa se cuelga en
`eglGetPlatformDisplay` (medido en el telefono). `tools/patch-mesa-g68.py` la
inserta en `prepare()` con `awk`, comprueba el fuente antes de tocar y sube
`pkgrel`. Ademas recorta el build a `panfrost,llvmpipe` (sin Vulkan, sin VAAPI,
sin Rust), que es lo unico que se usa.

### Rootfs

`device/` lleva el port (`device-xiaomi-ruby` y el stub `linux-xiaomi-ruby`) y
se copia sobre el clon de pmaports antes de `pmbootstrap install`. El kernel no
se compila: la imagen de boot sale aparte, del kernel del ROM.

## Uso

```sh
gh workflow run mesa.yml    -R <owner>/nura-build -f release_tag=nura-mesa-g68
gh workflow run rootfs.yml  -R <owner>/nura-build -f ui=plasma-mobile -f image_size_mb=4096 -f release_tag=nura-rootfs
```
