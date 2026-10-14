# Arquitectura de MarkPS4 Themes

MarkPS4 Themes es una aplicación nativa de PS4. La interfaz se dibuja en `ps4-app/tile-pkg/pms/main.cpp`; no depende de una página HTML para mostrar sus pantallas.

## Componentes principales

- `ps4-app/tile-pkg/pms/main.cpp`: renderizado, navegación con el mando y selección de paquetes.
- `ps4-app/tile-pkg/pms/theme_packages.h`: URL, identificador y tamaño de los paquetes seleccionados.
- `ps4-app/tile-pkg/Makefile` y `build-wsl.sh`: generación del PKG instalable.
- `.github/workflows/build-ps4-pkg.yml`: compilación automatizada y publicación del artefacto.

## Flujo de compilación

1. GitHub Actions prepara las herramientas de compilación.
2. Descarga y valida los dos paquetes de temas configurados en el workflow.
3. Genera los metadatos usados por la aplicación.
4. Compila y verifica el PKG nativo.
5. Publica el PKG como artefacto de Actions.

Los componentes auxiliares solo deben retirarse después de comprobar sus dependencias con este flujo.
