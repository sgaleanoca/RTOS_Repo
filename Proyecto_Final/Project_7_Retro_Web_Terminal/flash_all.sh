#!/bin/bash
# Script para flashear el proyecto completo incluyendo SPIFFS
# Uso: ./flash_all.sh [PORT]
# Ejemplo: ./flash_all.sh /dev/ttyUSB0

PORT=${1:-/dev/ttyUSB0}

echo "=========================================="
echo "Flasheando proyecto completo (incluyendo SPIFFS)"
echo "Puerto: $PORT"
echo "=========================================="

# Activar entorno ESP-IDF si no está activado
if ! command -v idf.py &> /dev/null; then
    echo "Activando entorno ESP-IDF..."
    # Intentar encontrar y activar export.sh
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        . "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
    elif [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/export.sh" ]; then
        . "$IDF_PATH/export.sh" >/dev/null 2>&1
    else
        echo "ERROR: No se encontró export.sh de ESP-IDF"
        echo "  Buscado en:"
        echo "    - $HOME/esp/esp-idf/export.sh"
        echo "    - \$IDF_PATH/export.sh"
        echo ""
        echo "  Solución: Activa el entorno ESP-IDF manualmente:"
        echo "    . \$HOME/esp/esp-idf/export.sh"
        exit 1
    fi
    
    # Verificar que idf.py esté disponible ahora
    if ! command -v idf.py &> /dev/null; then
        echo "ERROR: idf.py aún no está disponible después de activar ESP-IDF"
        exit 1
    fi
    echo "✓ Entorno ESP-IDF activado"
fi

# Verificar que el puerto existe
if [ ! -e "$PORT" ]; then
    echo "ERROR: El puerto $PORT no existe"
    exit 1
fi

# Verificar y limpiar build si está configurado para otra ruta del proyecto
echo ""
echo "1. Verificando configuración del build..."
CURRENT_DIR=$(pwd)
if [ -f "build/CMakeCache.txt" ]; then
    # Extraer la ruta del proyecto desde CMakeCache.txt
    CACHED_PROJECT_PATH=$(grep "^CMAKE_HOME_DIRECTORY:" build/CMakeCache.txt | cut -d "=" -f2 2>/dev/null)
    if [ -n "$CACHED_PROJECT_PATH" ] && [ "$CACHED_PROJECT_PATH" != "$CURRENT_DIR" ]; then
        echo "   ⚠ El build está configurado para otra ruta:"
        echo "      Configurado: $CACHED_PROJECT_PATH"
        echo "      Actual:      $CURRENT_DIR"
        echo "   Limpiando build..."
        idf.py fullclean
    fi
fi

# Construir el proyecto
echo ""
echo "2. Construyendo el proyecto..."
idf.py build

if [ $? -ne 0 ]; then
    echo "ERROR: Fallo al construir el proyecto"
    exit 1
fi

# Verificar si storage.bin existe (opcional, solo si hay carpeta front)
if [ ! -f "build/storage.bin" ]; then
    echo "⚠ build/storage.bin no encontrado (carpeta 'front' no existe, omitiendo SPIFFS)"
    SKIP_SPIFFS=true
else
    echo "✓ build/storage.bin encontrado ($(du -h build/storage.bin | cut -f1))"
    SKIP_SPIFFS=false
fi

# Leer la dirección de flash desde storage-flash_args (solo si se va a flashear SPIFFS)
if [ "$SKIP_SPIFFS" != "true" ]; then
    if [ -f "build/storage-flash_args" ]; then
        STORAGE_ADDR=$(grep "^0x" build/storage-flash_args | head -1)
        echo "✓ Dirección de SPIFFS: $STORAGE_ADDR"
    else
        STORAGE_ADDR="0x110000"
        echo "⚠ Usando dirección por defecto: $STORAGE_ADDR"
    fi
fi

# Flashear bootloader, particiones y aplicación
echo ""
echo "3. Flasheando bootloader, particiones y aplicación..."
idf.py -p $PORT flash

if [ $? -ne 0 ]; then
    echo "ERROR: Fallo al flashear con idf.py flash"
    exit 1
fi

# IMPORTANTE: Flashear SPIFFS manualmente (solo si existe storage.bin)
# Aunque FLASH_IN_APP está configurado, a veces no se incluye automáticamente
if [ "$SKIP_SPIFFS" = "true" ]; then
    echo ""
    echo "4. Omitiendo flasheo de SPIFFS (storage.bin no existe)"
else
    echo ""
    echo "4. Flasheando partición SPIFFS (storage.bin)..."
    echo "   Dirección: $STORAGE_ADDR"
    echo "   Archivo: build/storage.bin ($(du -h build/storage.bin | cut -f1))"

    # Intentar usar idf.py storage-flash primero (método recomendado)
    if idf.py storage-flash --help &>/dev/null; then
        echo "   Usando: idf.py storage-flash"
        idf.py -p $PORT storage-flash
        FLASH_RESULT=$?
    else
        # Si idf.py storage-flash no está disponible, usar esptool.py directamente
        echo "   Usando: esptool.py directamente"
        
        # Buscar esptool.py en el entorno ESP-IDF
        if [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/components/esptool_py/esptool/esptool.py" ]; then
            ESPTOOL="$IDF_PATH/components/esptool_py/esptool/esptool.py"
        elif [ -n "$IDF_PATH" ] && [ -f "$IDF_PATH/components/esptool_py/esptool/esptool.pyc" ]; then
            # Intentar con .pyc si .py no existe
            ESPTOOL="$IDF_PATH/components/esptool_py/esptool/esptool.py"
        elif command -v esptool.py &> /dev/null; then
            ESPTOOL="esptool.py"
        else
            echo "ERROR: esptool.py no encontrado"
            echo "  Buscado en:"
            echo "    - $IDF_PATH/components/esptool_py/esptool/esptool.py"
            echo "    - PATH (esptool.py)"
            echo ""
            echo "  Solución: Asegúrate de tener el entorno ESP-IDF activado:"
            echo "    . \$IDF_PATH/export.sh"
            exit 1
        fi
        
        echo "   Ejecutando: python $ESPTOOL"
        
        # Flashear SPIFFS usando los argumentos del build
        if [ -f "build/storage-flash_args" ]; then
            # Usar los argumentos generados por el build system
            python "$ESPTOOL" --chip esp32 --port $PORT --baud 921600 \
                --before default_reset --after hard_reset \
                write_flash $(cat build/storage-flash_args)
            FLASH_RESULT=$?
        else
            # Fallback: usar argumentos manuales
            python "$ESPTOOL" --chip esp32 --port $PORT --baud 921600 \
                --before default_reset --after hard_reset \
                write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
                $STORAGE_ADDR build/storage.bin
            FLASH_RESULT=$?
        fi
    fi

    if [ $FLASH_RESULT -eq 0 ]; then
        echo "✓ SPIFFS flasheado correctamente en $STORAGE_ADDR"
    else
        echo "ERROR: Fallo al flashear SPIFFS (código: $FLASH_RESULT)"
        echo ""
        echo "Intenta flashear manualmente con:"
        echo "  idf.py -p $PORT storage-flash"
        echo "O:"
        echo "  esptool.py --chip esp32 --port $PORT write_flash $STORAGE_ADDR build/storage.bin"
        exit 1
    fi
fi

echo ""
echo "=========================================="
echo "¡Flasheo completo!"
echo "=========================================="
echo ""
echo "Para ver los logs, ejecuta:"
echo "  idf.py -p $PORT monitor"
echo ""

