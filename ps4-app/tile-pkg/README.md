# MarkPS4 Themes — aplicación nativa para PS4

Este directorio compila el PKG instalable de **MarkPS4 Themes**:

`IV0000-PKGM00001_00-MARKPS4THEMES001.pkg`

## Archivos principales

- `pms/main.cpp`: interfaz nativa, navegación con el mando y selección/instalación de paquetes.
- `pms/theme_packages.h`: metadatos de los paquetes que GitHub Actions genera durante la compilación.
- `Makefile`: reglas de compilación del ejecutable y creación del PKG.
- `sce_sys/icon0.png`: icono que aparece en el menú de PS4.
- `../branding/`: logotipo e icono de la marca.

La interfaz se dibuja desde C++; no necesita una página web para mostrar el catálogo.

## Compilación local

Desde WSL, con las dependencias indicadas en `build-wsl.sh` instaladas:

```bash
bash ps4-app/tile-pkg/build-wsl.sh
```

## Compilación automática

En GitHub, abre **Actions → Build PS4 PKG MarkPS4 Themes → Run workflow**. Si termina correctamente, descarga el artefacto `MARKPS4-THEMES-PS4`.

El PKG final se crea en este directorio.
