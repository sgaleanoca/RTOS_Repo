# Documentación del Proyecto

Esta carpeta contiene la documentación generada automáticamente con Doxygen.

## Generar el PDF

Para generar el PDF de la documentación, ejecuta desde el directorio raíz del proyecto:

```bash
./generar_pdf.sh
```

### Requisitos

- Doxygen instalado
- pdflatex instalado (parte de texlive-latex-base)

### Instalación de dependencias

```bash
sudo apt-get update
sudo apt-get install doxygen texlive-latex-base texlive-latex-extra
```

## Archivos generados

Después de ejecutar el script, encontrarás:

- `Documentacion_Terminal_Web_Retro_ESP32.pdf` - PDF principal de la documentación
- `latex/refman.pdf` - PDF original generado por LaTeX
- `latex/` - Archivos fuente LaTeX (pueden eliminarse después de generar el PDF)

## Estructura

```
docs/
├── Documentacion_Terminal_Web_Retro_ESP32.pdf  (PDF principal)
├── latex/                                       (Archivos LaTeX)
│   ├── refman.pdf
│   ├── refman.tex
│   └── ...
└── README.md                                    (Este archivo)
```
