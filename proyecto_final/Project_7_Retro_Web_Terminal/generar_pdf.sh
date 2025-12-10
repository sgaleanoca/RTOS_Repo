#!/bin/bash
# Script para generar documentación PDF con Doxygen
# Requiere: doxygen y pdflatex instalados
# El PDF se guardará en la carpeta docs/

echo "=========================================="
echo "Generando documentación PDF con Doxygen"
echo "=========================================="

# Verificar que doxygen esté instalado
if ! command -v doxygen &> /dev/null; then
    echo "ERROR: Doxygen no está instalado."
    echo "Instala con: sudo apt-get install doxygen"
    exit 1
fi

# Verificar que pdflatex esté instalado
if ! command -v pdflatex &> /dev/null; then
    echo "ERROR: pdflatex no está instalado."
    echo "Instala con: sudo apt-get install texlive-latex-base texlive-latex-extra"
    exit 1
fi

# Crear directorio docs si no existe
if [ ! -d "docs" ]; then
    echo "Creando directorio docs/..."
    mkdir -p docs
fi

# Generar documentación LaTeX
echo ""
echo "Paso 1: Generando archivos LaTeX con Doxygen..."
doxygen Doxyfile

if [ $? -ne 0 ]; then
    echo "ERROR: Fallo al generar archivos LaTeX"
    exit 1
fi

# Verificar que se generaron los archivos LaTeX
if [ ! -d "docs/latex" ]; then
    echo "ERROR: No se generó el directorio docs/latex/"
    exit 1
fi

# Compilar PDF
echo ""
echo "Paso 2: Compilando PDF desde LaTeX..."
cd docs/latex

# Primera pasada de pdflatex
echo "  - Primera pasada de pdflatex..."
pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1

# Generar índice
echo "  - Generando índice..."
makeindex refman.idx > /dev/null 2>&1

# Segunda pasada de pdflatex
echo "  - Segunda pasada de pdflatex..."
pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1

# Tercera pasada (para referencias cruzadas)
echo "  - Tercera pasada de pdflatex (referencias cruzadas)..."
pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1

cd ../..

# Verificar que el PDF se generó
if [ -f "docs/latex/refman.pdf" ]; then
    # Copiar PDF a docs/ con nombre descriptivo
    cp docs/latex/refman.pdf "docs/Documentacion_Terminal_Web_Retro_ESP32.pdf"
    echo ""
    echo "=========================================="
    echo "✓ PDF generado exitosamente!"
    echo "=========================================="
    echo "Archivo principal: docs/Documentacion_Terminal_Web_Retro_ESP32.pdf"
    echo "Tamaño: $(du -h docs/Documentacion_Terminal_Web_Retro_ESP32.pdf | cut -f1)"
    echo ""
    echo "El PDF también está disponible en: docs/latex/refman.pdf"
    echo ""
    echo "Estructura de archivos generados:"
    echo "  docs/"
    echo "    ├── Documentacion_Terminal_Web_Retro_ESP32.pdf  (PDF principal)"
    echo "    └── latex/"
    echo "        └── refman.pdf  (PDF original)"
else
    echo ""
    echo "ERROR: No se pudo generar el PDF"
    echo "Revisa los logs en docs/latex/ para más detalles"
    exit 1
fi
