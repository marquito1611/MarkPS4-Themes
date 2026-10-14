# MarkPS4 Themes

Aplicación nativa para PS4 con jailbreak que permite seleccionar e instalar los paquetes de temas incluidos en el catálogo de MarkPS4 Themes.

## Proyecto actual

- **Aplicación e interfaz nativa:** `ps4-app/tile-pkg/pms/main.cpp`
- **Metadatos de los paquetes:** `ps4-app/tile-pkg/pms/theme_packages.h` (GitHub Actions los regenera a partir de los PKG usados en la compilación).
- **Icono y marca:** `ps4-app/branding/` y `ps4-app/tile-pkg/sce_sys/icon0.png`.
- **Compilación del PKG:** `ps4-app/tile-pkg/build-wsl.sh`.
- **Compilación automática:** `.github/workflows/build-ps4-pkg.yml`.

La interfaz de la aplicación se dibuja directamente en C++ y se controla con el mando. La antigua interfaz web ya no forma parte del producto.

## Compilar

En GitHub, abre **Actions → Build PS4 PKG MarkPS4 Themes** y ejecuta el workflow con **Run workflow**. Cuando termine correctamente, descarga el artefacto **MARKPS4-THEMES-PS4**.

El PKG generado se llama:

`IV0000-PKGM00001_00-MARKPS4THEMES001.pkg`

## Estructura importante

- `ps4-app/tile-pkg/`: aplicación de PS4 e instalador de paquetes.
- `ps4-app/branding/`: recursos de marca.
- `art-src/`: arte de origen usado por la compilación.
- `.github/workflows/`: compilación automática.

No se deben eliminar dependencias del compilador ni de la instalación sin comprobar antes que no las use el flujo de compilación.
