# Baraja

**Brassage en tiempo real** para VST3, CLAP, AU y como aplicación independiente, en macOS y Windows.

Baraja guarda los últimos segundos de lo que entra en una memoria y la recorre tomando fragmentos de tamaño, altura, sentido y posición estéreo variables, que superpone según la densidad elegida. La memoria se puede congelar para seguir barajando lo que quedó guardado aunque la pista quede en silencio.

![Interfaz de Baraja](docs/captura.png)

## Descargar e instalar

1. Entrá a **[Releases](../../releases)** y bajá el paquete de tu sistema: `Baraja-…-macOS.zip` o `Baraja-…-Windows.zip`.
2. Descomprimilo y seguí las instrucciones del archivo `LEEME.txt` que viene adentro. Hay un instalador para cada sistema.

Las versiones de prueba, que todavía no se publicaron como Release, están en la pestaña **[Actions](../../actions)**: abrí la última compilación exitosa y bajá el paquete desde *Artifacts* (hace falta una cuenta de GitHub).

## Parámetros

| Grupo | Parámetro | Qué hace |
|---|---|---|
| Fragmentos | Tamaño | Duración media de cada fragmento (5 ms a 1 s). |
| | Variación del tamaño | Cuánto cambia la duración de un fragmento a otro. |
| | Densidad | Cuántos fragmentos suenan superpuestos en promedio. |
| | Irregularidad rítmica | 0 = fragmentos a intervalos regulares; 100 = totalmente irregulares. |
| | Suavidad de ventana | Forma de la envolvente de cada fragmento: de bordes abruptos a campana. |
| Memoria | Profundidad | Hasta qué punto del pasado se toman fragmentos (20 ms a 8 s). |
| | Congelar memoria | Deja de grabar y sigue barajando lo guardado. |
| Altura y sentido | Transposición fija | Transpone todos los fragmentos en la misma proporción (±24 semitonos). |
| | Variación aleatoria | Cada fragmento recibe además una altura al azar dentro de este rango. |
| | Deriva aleatoria / velocidad | La transposición fija vaga sola en el tiempo. |
| | Fragmentos invertidos | Probabilidad de que un fragmento se lea al revés. |
| | Sentido de la variación | Hacia ambos lados, solo hacia arriba o solo hacia abajo. |
| | Alturas posibles | Libres, cuantizadas a semitonos o a octavas. |
| Espacio y salida | Dispersión estéreo | Cuánto se reparten los fragmentos entre izquierda y derecha. |
| | Amplitud aleatoria | Variación de intensidad entre fragmentos. |
| | Mezcla | Proporción entre señal original y procesada. |
| | Salida | Ganancia final. |

Doble clic sobre un control lo vuelve a su valor inicial. Los ajustes de fábrica (Granizo, Nube lenta, Tartamudeo, Coral, Espejo, Octavas, Deriva, Enjambre) también aparecen en la lista de presets del DAW.

## Compilar desde el código

Hace falta CMake 3.22 o posterior y un compilador de C++17: Xcode o sus Command Line Tools en macOS, Visual Studio 2022 en Windows. JUCE y la extensión CLAP se descargan solos.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

Cada cambio que se sube a `main` se compila automáticamente para macOS (universal) y Windows, se prueba y se valida con [pluginval](https://github.com/Tracktion/pluginval). Para publicar una versión, se crea una etiqueta `vX.Y.Z`.

### Estructura

- `Source/BarajaEngine.h`: el motor de audio, independiente de JUCE.
- `Source/Parameters.h`: parámetros y ajustes de fábrica.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: el plugin y su interfaz.
- `tests/`: prueba del motor y herramienta de captura de la interfaz.
- `instalar/`: instaladores e instrucciones que viajan en cada paquete.

## Licencia

Copyright © 2026 Lautaro Martín Vieyra.
Baraja es software libre bajo la [GNU Affero General Public License 3.0](LICENSE) o posterior: se puede usar, estudiar, compartir y modificar, siempre que las versiones modificadas que se distribuyan mantengan la misma licencia y su código abierto.

Usa [JUCE](https://juce.com) (AGPL 3.0) y [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) (MIT).
VST es una marca registrada de Steinberg Media Technologies GmbH.
