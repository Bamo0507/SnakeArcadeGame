#include <Adafruit_NeoPixel.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <queue> 
#include <random>
#include "esp_system.h" 
#include <ESP.h>

#define PIN_NEO_PIXEL 16      // El pin GPIO16 del ESP32 conectado a NeoPixel
#define PIN_LIGHT_SENSOR 36   // El pin GPIO36 del ESP32 conectado al sensor de luz
#define NUM_PIXELS 64         // El número de LEDs (pixels) en la tira NeoPixel LED


//Propuesta de pines para input de los botones
#define PIN_RED_BUTTON 12 //Input de 'w'
#define PIN_GREEN_BUTTON 4 //Input de 'a'
#define PIN_BLUE_BUTTON 14 //Input de 's'
#define PIN_YELLOW_BUTTON 2 //Input de 'd'
#define PIN_WHITE_BUTTON 13 //Intput de reiniciar

//Variables para controlar el tiempo
unsigned long tiempoUltimoMovimiento = 0;
unsigned long intervaloMovimiento = 400; //estos serían milisegundos

// Dimensiones iniciales del terreno
int ancho = 8; 
int largo = 8; 
int nivel = 1;  // Nivel actual
int puntaje = 0; // Puntaje del jugador

// Definiciones de las celdas
const int PARED = 3;
const int CAMINO = 0;
const int ENTRADA = 5;
const int SALIDA = 6;

// Dimensiones del laberinto
const int ANCHO = 8;
const int ALTO = 8;

// Matriz del terreno
std::vector<std::vector<int>> terreno; // 0 = camino, 1 = manzana, 2 = serpiente, 3 = pared, 5 = entrada, 6 = salida

// Estructura para manejar lógica de matriz
struct Coordenada {
    int x, y;
    Coordenada& operator=(const Coordenada& other) volatile {
        x = other.x;
        y = other.y;
        return const_cast<Coordenada&>(*this);

    }
};

// LÓGICA POSICIONAL INICIAL
std::vector<Coordenada> serpiente;
volatile Coordenada direccion = {0, 0};
std::vector<Coordenada> manzanas; // Lista de manzanas
bool game_over = false;

// Declaración de funciones
void generarManzanas(int cantidad);
void iniciarTerreno();
void imprimirTerreno();
void actualizarTerreno();
void generarLaberinto(int width, int height);
void tallarLaberinto(int x, int y);
std::vector<Coordenada> obtenerCeldasBorde();
Coordenada seleccionarCeldaAleatoria(const std::vector<Coordenada>& celdasBorde);
std::vector<std::pair<int, int>> obtenerDireccionesAleatorias();

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);

//CONFIGURACIÓN DE BOTONES Y SU LÓGICA SOBRE LA MATRIZ
//IRAM_ATTR es algo propio de la ESP32 para poder hacer interrupciones y así leer el input

//se agrega un Debounce en la lectura del botón, puede pasar que con un push se detecten varios inputs
volatile unsigned long last_interrupt_time_w = 0;
volatile unsigned long last_interrupt_time_a = 0;
volatile unsigned long last_interrupt_time_s = 0;
volatile unsigned long last_interrupt_time_d = 0;
const unsigned long debounce_delay = 200; //200 mili segundos para el debounce

//PROBABLEMENTE VAYA A DAR PROBLEMA AL INICIO PUES LOS VALORES ESTARÁN EN 0,0 - hay que ver eso 
//Movernos hacia arriba 'w'
void IRAM_ATTR cambiarDireccionW(){
    if (game_over) return;  

    unsigned long current_time = millis();
    if (current_time - last_interrupt_time_w > debounce_delay) {
        if (direccion.y != 1) { // Evita que la serpiente vaya hacia abajo
          direccion = {0, -1}; // 'w' = arriba
          last_interrupt_time_w = current_time;
        }
      }
}

void IRAM_ATTR cambiarDireccionA() {
  if (game_over) return; 

  unsigned long current_time = millis();
  if (current_time - last_interrupt_time_a > debounce_delay) {
    if (direccion.x != 1) { // Evita que la serpiente vaya hacia la derecha
      direccion = {-1, 0}; // 'a' = izquierda
      last_interrupt_time_a = current_time;
    }
  }
}

void IRAM_ATTR cambiarDireccionS() {
  if (game_over) return; 

  unsigned long current_time = millis();
  if (current_time - last_interrupt_time_s > debounce_delay) {
    if (direccion.y != -1) { // Evita que la serpiente vaya hacia arriba
      direccion = {0, 1}; // 's' = abajo
      last_interrupt_time_s = current_time;
    }
  }
}

void IRAM_ATTR cambiarDireccionD() {
  if (game_over) return; 

  unsigned long current_time = millis();
  if (current_time - last_interrupt_time_d > debounce_delay) {
    if (direccion.x != -1) { // Evita que la serpiente vaya hacia la izquierda
      direccion = {1, 0}; // 'd' = derechacian
      last_interrupt_time_d = current_time;
    }
  }
}

void IRAM_ATTR  reiniciarJuego(){
  if (!game_over) return; 
  NeoPixel.clear();  // apaga todos los LEDs
  delay(500);
  puntaje = 0;
  nivel = 1;
  intervaloMovimiento = 400; // Resetear velocidad
  game_over = false;
  delay(500)
  iniciarTerreno();
}
//----------------------------------------------------

void setup() {
  NeoPixel.begin();  // inicializar el objeto NeoPixel (REQUERIDO)
  Serial.begin(9600); // abrir el puerto serie a 9600 bps:
  //CONFIGURACIÓN DE PINES COMO INPUTS
  pinMode(PIN_RED_BUTTON, INPUT_PULLUP);
  pinMode(PIN_GREEN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BLUE_BUTTON, INPUT_PULLUP);
  pinMode(PIN_YELLOW_BUTTON, INPUT_PULLUP);
  pinMode(PIN_WHITE_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LIGHT_SENSOR, INPUT);


    //ASOCIAR INTERRUPCIONES A UN PIN ESPECÍFICO
    /*
    attachInterrupt - conecta una interrupción a un pin
    digitalPinToInterrupt - convierte un pin digital en una interrupción
    cambiarDireccionX - método definido arriba para lógica matricial
    FALLING - hace el cambio cuando pasamos de HIGH a LOW o de 0 a 1
    */
    attachInterrupt(digitalPinToInterrupt(PIN_RED_BUTTON), cambiarDireccionW, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_GREEN_BUTTON), cambiarDireccionA, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BLUE_BUTTON), cambiarDireccionS, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_YELLOW_BUTTON), cambiarDireccionD, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_WHITE_BUTTON), reiniciarJuego, FALLING);

    int Light_value = analogRead(PIN_LIGHT_SENSOR); // valor entre 0 a 4095
    int Brightness = std::floor((Light_value / 16));
    if(Brightness < 20){
        Brightness = 10;
      }
    
    NeoPixel.setBrightness(Brightness); // un valor de 0 a 255

    iniciarTerreno();
}

void loop() {
    int Light_value = analogRead(PIN_LIGHT_SENSOR); // valor entre 0 a 4095
    int Brightness = std::floor((Light_value / 16));
    if(Brightness < 20){
        Brightness = 10;
      }
    
    NeoPixel.setBrightness(Brightness); // un valor de 0 a 255
    Serial.println(Brightness);
        
    unsigned long tiempoActual = millis(); //tiempo del programa desde que se ejecuto

    //Verificar si es tiempo de mover a la serpiente
    if(tiempoActual - tiempoUltimoMovimiento >= intervaloMovimiento){
        tiempoUltimoMovimiento = tiempoActual; //actualizamos el valor para que no haya problema al leer el siguiente movimiento

        //Como se cumplió la condición hay que actualizar la dirección
        if(direccion.x != 0 || direccion.y != 0){
            moverSerpiente();
        }
    }
    
    // Manejar el fin del juego
      if (game_over) {
        Serial.println("Juego Terminado. Esperando reinicio...");
        for (int pixel = 0; pixel < 8; pixel++) {          
          int Light_value = analogRead(PIN_LIGHT_SENSOR); // valor entre 0 a 4095
          int Brightness = std::floor((Light_value / 16));
          if(Brightness < 20){
              Brightness = 10;
            }
          
          NeoPixel.setBrightness(Brightness); // un valor de 0 a 255

          NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 255, 255));  
          NeoPixel.setPixelColor((8 + pixel), NeoPixel.Color(255, 255, 255)); 
          NeoPixel.setPixelColor((16 + pixel), NeoPixel.Color(255, 255, 255));  
          NeoPixel.setPixelColor((24 + pixel), NeoPixel.Color(255, 255, 255));  
          NeoPixel.setPixelColor((32 + pixel), NeoPixel.Color(255, 255, 255)); 
          NeoPixel.setPixelColor((40 + pixel), NeoPixel.Color(255, 255, 255));  
          NeoPixel.setPixelColor((48 + pixel), NeoPixel.Color(255, 255, 255));  
          NeoPixel.setPixelColor((56 + pixel), NeoPixel.Color(255, 255, 255)); 
          NeoPixel.show();                                         

          if(pixel > 0){
            NeoPixel.setPixelColor((pixel - 1), NeoPixel.Color(255, 0, 0)); 
            NeoPixel.setPixelColor((8 + pixel - 1), NeoPixel.Color(255, 0, 0)); 
            NeoPixel.setPixelColor((16 + pixel - 1), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((24 + pixel - 1), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((32 + pixel - 1), NeoPixel.Color(255, 0, 0)); 
            NeoPixel.setPixelColor((40 + pixel - 1), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((48 + pixel - 1), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((56 + pixel - 1), NeoPixel.Color(255, 0, 0));  
          }
          NeoPixel.show(); 
          
          if(pixel == 7){
            NeoPixel.setPixelColor((7), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((15), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((23), NeoPixel.Color(255, 0, 0)); 
            NeoPixel.setPixelColor((31), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((39), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((47), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((55), NeoPixel.Color(255, 0, 0));  
            NeoPixel.setPixelColor((63), NeoPixel.Color(255, 0, 0));  
            NeoPixel.show(); 
          }
          delay(100);  
        }
      }
  
}

/*
Función para controlar el movimiento de la serpiente
*/
void moverSerpiente(){
    // Obtener la posición actual de la cabeza de la serpiente, se suma a x y a y pasado el nuevo
    Coordenada nueva_cabeza = {serpiente[0].x + direccion.x, serpiente[0].y + direccion.y};

    //Verificación de condiciones
    if (nueva_cabeza.x < 0 || nueva_cabeza.x >= ANCHO || nueva_cabeza.y < 0 || nueva_cabeza.y >= ALTO){
        Serial.println("Jugador perdió se salió del área de juego");
        game_over = true;
        ESP.restart();
        return;
    }

    //Verificar si la serpiente colisiona con paredes o consigo misma
    if(terreno[nueva_cabeza.y][nueva_cabeza.x] == PARED || terreno[nueva_cabeza.y][nueva_cabeza.x] == 2){
        Serial.println("Jugador perdió se chocó con pared o consigo mismo");
        game_over = true;
        ESP.restart();
        return;
    }

    //Verificar si la serpiente completó el nivel
    if(terreno[nueva_cabeza.y][nueva_cabeza.x] == SALIDA){
        aumentarNivel();
        return; //dejamos de leer movimiento
    }

    //Verificar si la serpiente come manzanas y manejar evento
    bool manzanaComida = false; //reinicia el valor constantemente
    for(auto it = manzanas.begin(); it != manzanas.end(); it++){
        if(nueva_cabeza.x == it->x && nueva_cabeza.y == it->y){
            manzanaComida = true;
            manzanas.erase(it); //nos volamos la que se comió
            puntaje += 1;
            break;
        }
    }
    if(manzanaComida){
        serpiente.insert(serpiente.begin(), nueva_cabeza); //agregamos un cuadro sobre la serpiente
    } else {
        serpiente.insert(serpiente.begin(), nueva_cabeza); //mover serpiente
        serpiente.pop_back(); //eliminar la cola de la serpiente
    }

    actualizarTerreno();
    imprimirTerreno();
}

/*
 Función para auemntar el nivel - manejar lógica de reucir intervalo de lectura
*/
void aumentarNivel(){
    Serial.println("El jugador completó un nivel.");
    nivel++;
    puntaje += 10;
    if (intervaloMovimiento > 100) { //solo si todavía estamos con 100 milisegundos le bajamos al tiempo de lectura para que sea más rápido
        intervaloMovimiento -= 5;            
    } 

    //Generar nuevo laberinto
    iniciarTerreno();
}

/*
 * Función para imprimir el terreno en los LEDs
 */
void imprimirTerreno() {

    //Control de brillo automatico en cada impresion
    int Light_value = analogRead(PIN_LIGHT_SENSOR); // valor entre 0 a 4095
    int Brightness = std::floor((Light_value / 16));
    if(Brightness < 20){
        Brightness = 10;
      }
    
    NeoPixel.setBrightness(Brightness); // un valor de 0 a 255

    for (int y = 0; y < largo; y++) {
        for (int x = 0; x < ancho; x++) {
          int pixel = (y * 8) + x;
          if (serpiente.size() > 0 && serpiente[0].x == x && serpiente[0].y == y) { // Cabeza de la serpiente
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(40, 120, 40)); // Verde oscuro
          } else if (terreno[y][x] == 2) { // Cuerpo de la serpiente
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(0, 255, 0)); // Verde
          } else if (terreno[y][x] == 1) { // Manzana
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 0, 0)); // Rojo
          } else if (terreno[y][x] == PARED) { // Pared
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(0, 255, 255)); // Cian
          } else if (terreno[y][x] == ENTRADA) { // Entrada
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 255, 0)); // Amarillo
          } else if (terreno[y][x] == SALIDA) { // Salida
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(255, 0, 255)); // Magenta
          } else { // Camino libre
            NeoPixel.setPixelColor(pixel, NeoPixel.Color(0, 0, 255)); // Azul
          }
        }
      }
    NeoPixel.setPixelColor(0, NeoPixel.Color(255, 255, 255)); // blanco
    NeoPixel.show();
}

/*
 * Función para generar múltiples manzanas en posiciones aleatorias
 */
void generarManzanas(int cantidad) {
    manzanas.clear();

    for (int i = 0; i < cantidad; ++i) {
        int x, y;
        do {
            x = rand() % (ancho - 2) + 1;
            y = rand() % (largo - 2) + 1;
        } while (terreno[y][x] != CAMINO);
        manzanas.push_back({x, y});
        terreno[y][x] = 1;
    }
}

/*
 * Función para generar el laberinto usando el algoritmo de Backtracking recursivo
 */
void generarLaberinto(int width, int height) {
    // Reiniciar el terreno con paredes
    terreno = std::vector<std::vector<int>>(height, std::vector<int>(width, PARED));

    // Obtener todas las celdas de borde
    std::vector<Coordenada> celdasBorde = obtenerCeldasBorde();

    // Seleccionar aleatoriamente la entrada y la salida
    Coordenada entrada = seleccionarCeldaAleatoria(celdasBorde);
    Coordenada salida;
    do {
        salida = seleccionarCeldaAleatoria(celdasBorde);
    } while ((entrada.x == salida.x) && (entrada.y == salida.y));

    // Ajustar la entrada si está en una posición par
    if (entrada.x % 2 == 0 && entrada.y % 2 == 0) {
        if (entrada.x == 0) entrada.x += 1;
        if (entrada.x == ancho - 1) entrada.x -= 1;
        if (entrada.y == 0) entrada.y += 1;
        if (entrada.y == largo - 1) entrada.y -= 1;
    }

    // Tallar el laberinto comenzando desde la entrada
    tallarLaberinto(entrada.x, entrada.y);

    terreno[1][0] = 0;
    terreno[0][1] = 0;
    terreno[1][7] = 0;
    terreno[6][0] = 0;
    terreno[7][1] = 0;
    terreno[0][6] = 0;
    terreno[6][7] = 0;
    terreno[7][6] = 0;

    // Establecer entrada y salida en el laberinto
    terreno[entrada.y][entrada.x] = ENTRADA;
    terreno[salida.y][salida.x] = SALIDA;

    bool esValido = VerificarLaberinto();

    if(esValido == false){
      generarLaberinto(ancho, largo);
    }
}

/*
 * Función para verificar si existe un camino entre la entrada y la salida en el laberinto.
 * Utiliza una búsqueda en anchura (BFS) para explorar posibles caminos desde la entrada.
 */
bool VerificarLaberinto() {
    // Encuentra la posición de la entrada y la salida en el laberinto
    Coordenada entrada, salida;
    bool entradaEncontrada = false, salidaEncontrada = false;
    
    for (int y = 0; y < ALTO; ++y) {
        for (int x = 0; x < ANCHO; ++x) {
            if (terreno[y][x] == ENTRADA) {
                entrada = {x, y};
                entradaEncontrada = true;
            }
            if (terreno[y][x] == SALIDA) {
                salida = {x, y};
                salidaEncontrada = true;
            }
        }
    }

    // Si no se encontraron entrada o salida, el laberinto no es válido
    if (!entradaEncontrada || !salidaEncontrada) {
        Serial.println("Laberinto no tiene entrada o salida.");
        return false;
    }

    // Búsqueda en anchura (BFS) para verificar el camino
    std::queue<Coordenada> cola;
    std::vector<std::vector<bool>> visitado(ALTO, std::vector<bool>(ANCHO, false));
    cola.push(entrada);
    visitado[entrada.y][entrada.x] = true;

    // Direcciones de movimiento posibles: arriba, abajo, izquierda, derecha
    std::vector<Coordenada> direcciones = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

    while (!cola.empty()) {
        Coordenada actual = cola.front();
        cola.pop();

        // Si hemos llegado a la salida, el laberinto es válido
        if (actual.x == salida.x && actual.y == salida.y) {
            return true;
        }

        // Explora las celdas vecinas
        for (const auto& dir : direcciones) {
            int nuevoX = actual.x + dir.x;
            int nuevoY = actual.y + dir.y;

            // Comprueba si la celda vecina está dentro de los límites y es un camino
            if (nuevoX >= 0 && nuevoX < ANCHO && nuevoY >= 0 && nuevoY < ALTO &&
                (terreno[nuevoY][nuevoX] == CAMINO || terreno[nuevoY][nuevoX] == SALIDA) &&
                !visitado[nuevoY][nuevoX]) {
                
                visitado[nuevoY][nuevoX] = true;
                cola.push({nuevoX, nuevoY});
            }
        }
    }

    // Si no se alcanzó la salida, el laberinto no es válido
    Serial.println("No hay camino desde la entrada hasta la salida.");
    return false;
}


/*
 * Función para obtener todas las celdas de borde
 */
std::vector<Coordenada> obtenerCeldasBorde() {
    std::vector<Coordenada> celdasBorde;
    for (int x = 0; x < ancho; x++) {
        celdasBorde.push_back({x, 0});           // Borde superior
        celdasBorde.push_back({x, largo - 1});   // Borde inferior
    }
    for (int y = 1; y < largo - 1; y++) {
        celdasBorde.push_back({0, y});           // Borde izquierdo
        celdasBorde.push_back({ancho - 1, y});   // Borde derecho
    }
    return celdasBorde;
}

/*
 * Función para seleccionar aleatoriamente una celda de borde
 */
Coordenada seleccionarCeldaAleatoria(const std::vector<Coordenada>& celdasBorde) {
    int index = esp_random() % celdasBorde.size();
    return celdasBorde[index];
}

/*
 * Función para mezclar direcciones aleatoriamente usando esp_random()
 */
std::vector<std::pair<int, int>> obtenerDireccionesAleatorias() {
    std::vector<std::pair<int, int>> direcciones = {
        {0, -1},  // Norte
        {1, 0},   // Este
        {0, 1},   // Sur
        {-1, 0}   // Oeste
    };

    // Mezclar direcciones usando esp_random()
    for (int i = direcciones.size() - 1; i > 0; --i) {
        int j = esp_random() % (i + 1);
        std::swap(direcciones[i], direcciones[j]);
    }

    return direcciones;
}

/*
 * Función para tallar el laberinto recursivamente
 */
void tallarLaberinto(int x, int y) {
    terreno[y][x] = CAMINO; // Marcar como camino

    std::vector<std::pair<int, int>> direcciones = obtenerDireccionesAleatorias();

    for (auto dir : direcciones) {
        int nx = x + dir.first * 2;
        int ny = y + dir.second * 2;

        // Asegurar que la nueva posición esté dentro de los límites y sea una pared
        if (nx >= 0 && nx < ancho && ny >= 0 && ny < largo && terreno[ny][nx] == PARED) {
            terreno[y + dir.second][x + dir.first] = CAMINO; // Eliminar la pared intermedia
            tallarLaberinto(nx, ny);
        }
    }
}

/*
 * Función para inicializar el terreno y la serpiente
 */
void iniciarTerreno() {
  // Generar el laberinto
  generarLaberinto(ancho, largo);

  // Reiniciar la serpiente
  serpiente.clear();

  // Encontrar la entrada y posicionar la serpiente
  bool encontrada = false;
  Coordenada entrada;
  for (int y = 0; y < largo && !encontrada; y++) {
    for (int x = 0; x < ancho && !encontrada; x++) {
      if (terreno[y][x] == ENTRADA) { //encontramos coordenada de entrada
        entrada = {x, y};
        serpiente.push_back(entrada); //agregamos una cabeza sobre coordenada de entrada
        encontrada = true;
      }
    }
  }

  /*
  // Agregar un segundo segmento detrás de la cabeza si es posible
    //Se verifica si en alguna de las direcciones arriba, abajo, izquierda o derecha hay un camino, si está, agregamos un segmento detrás de la cabeza
  Coordenada segundo_segmento = {entrada.x, entrada.y};
  if (direccion.y != -1 && entrada.y < largo - 1 && terreno[entrada.y + 1][entrada.x] == CAMINO) {
    segundo_segmento.y += 1;
  } else if (direccion.y != 1 && entrada.y > 0 && terreno[entrada.y - 1][entrada.x] == CAMINO) {
    segundo_segmento.y -= 1;
  } else if (direccion.x != 1 && entrada.x > 0 && terreno[entrada.y][entrada.x - 1] == CAMINO) {
    segundo_segmento.x -= 1;
  } else if (direccion.x != -1 && entrada.x < ancho - 1 && terreno[entrada.y][entrada.x + 1] == CAMINO) {
    segundo_segmento.x += 1;
  }

  // Verificar si el segundo segmento es válido
    //detrás del agregado antes vemos si ya hay camino libre y se hace el push_back
  if (segundo_segmento.x != entrada.x || segundo_segmento.y != entrada.y) {
    serpiente.push_back(segundo_segmento);
  }
  */

  direccion = {0, 0}; // Reiniciar dirección para siguietne movimiento

  // Generar manzanas (por ejemplo, 3 manzanas por nivel)
  generarManzanas(3);

  // Actualizar el terreno con la posición de la serpiente y las manzanas
  actualizarTerreno();

  // Imprimir el terreno en los LEDs
  imprimirTerreno();
}

/*
 * Función para actualizar el terreno con la posición de la serpiente y las manzanas
 */
void actualizarTerreno() {
    //BLOQUEAR acceso al terreno mientras se actualiza (reconocimiento de inputs)
    noInterrupts();

    //Limpiar el terreno(dejar solo los muros, 'E', 'S')
    for (int y = 0; y < largo; y++) {
        for (int x = 0; x < ancho; x++) {
          if (terreno[y][x] == CAMINO || terreno[y][x] == PARED || terreno[y][x] == ENTRADA || terreno[y][x] == SALIDA) {
            // Mantener el estado actual
          } else {
            terreno[y][x] = CAMINO; // Limpiar otras marcas
          }
        }
      }

    // Colocar la serpiente en el terreno - colocamos sobre el terreno los 2s
      for (auto &segmento : serpiente) {
        terreno[segmento.y][segmento.x] = 2;
      }

      // Colocar las manzanas - colocamos sobre el terreno los 1s
      for (auto &manzana : manzanas) {
        terreno[manzana.y][manzana.x] = 1;
      }

      interrupts(); // Rehabilitar interrupciones - ya se leen inputs
}

