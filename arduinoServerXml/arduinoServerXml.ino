#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <DHT.h>
#include <DS3231.h>
#include <EEPROM.h>
//
//#include <LiquidCrystal_I2C.h>
// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ 60

DHT dht1(2, DHT11);
DHT dht2(3, DHT11);
File HMTL_file;

//LiquidCrystal_I2C lcd(0x27, 16, 2);
DS3231 clock;
RTCDateTime dt;

// MAC address from Ethernet shield sticker under board
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 0, 20);   // IP address, may need to change depending on network
EthernetServer server(80);       // create a server at port 80
File webFile;                    // the web page file on the SD card
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;
boolean MODOAG_state[2] = {0}; // index into HTTP_req buffer
boolean LED_state[2] = {0};    // stores the states of the LEDs

boolean AUTOMANUAL_state[1] = {0};
/*
//controla el modo horario o desactivacion del armario grande
int estadoAgModoLuz = EEPROM.read(0);
//controla el modo horario o desactivacion del armario pequeño
int estadoApModoLuz = EEPROM.read(1);
*/

//tiempos
int periodo = 1000;
unsigned long TiempoAhora1;
unsigned long TiempoAhora2 = 0;
unsigned long TiempoAhora3 = 0;
unsigned long TiempoAhora4 = 0;

void setup()
{
    //EEPROM.write(0, 1);
    //EEPROM.write(1, 1);
    // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);

    Serial.begin(9600); // for debugging
    clock.begin();
    //clock.setDateTime(__DATE__, __TIME__); //--establece la hora, solo la primera vez q se sube el codigo.                  /// clock.begin();
    dht1.begin();
    dht2.begin();
    //inicializar el lcd
    /* lcd.init();
    lcd.backlight();
    lcd.setCursor(3, 0);
    lcd.print("--MEJURDINO--");
    delay(1000);*/

    // initialize SD card
    Serial.println("Initializing SD card...");
    if (!SD.begin(4))
    {
        Serial.println("ERROR - SD card initialization failed!");
        return; // init failed
    }
    Serial.println("SUCCESS - SD card initialized.");
    // check for index.htm file
    if (!SD.exists("index.htm"))
    {
        Serial.println("ERROR - Can't find index.htm file!");
        return; // can't find index file
    }
    Serial.println("SUCCESS - Found index.htm file.");
    // switches on pins 2, 3 and 5
    pinMode(2, INPUT);
    //pinMode(3, INPUT);
    // pinMode(5, INPUT);
    // LEDs
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    pinMode(8, OUTPUT);
    pinMode(9, OUTPUT);

    Ethernet.begin(mac, ip); // initialize Ethernet device
                             // Verificar que el Ethernet Shield está correctamente conectado.
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        Serial.println("Ethernet shield no presente :(");
        while (true)
        {
            delay(1); // do nothing, no point running without Ethernet hardware
        }
    }

    if (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("El cable Ethernet no está conectado o está defectuoso.");
    }

    server.begin();
    Serial.print(F("Server Started...\nLocal IP: "));
    Serial.println(Ethernet.localIP());
}

void loop()
{

    registroDatos();

    //si es igual a 1 activamos los modos horarios
    if (AUTOMANUAL_state[0] == 1)
    {
        Serial.println("dentro de if de automanualState == 1");
        //si alguno de los modos 12 y 18 esta ON activa la funcion de control del armario grande
        if (MODOAG_state[0] == 1 || MODOAG_state[1])
        {
            control_AG();
        }
        else
        {
            Serial.println("nINGUN MODO SELECIONADO");

            //foco1 = lado izquierdo del armario
            LED_state[0] = 0; // save LED state
            digitalWrite(8, LOW);
            //foco 2 = lado derecho del armario
            LED_state[1] = 0; // save LED state
            digitalWrite(9, LOW);
        }
    }

    listenForClients();
}

void listenForClients()
{
    EthernetClient client = server.available(); // try to get client

    if (client)
    { // got client?
        boolean currentLineIsBlank = true;
        while (client.connected())
        {
            if (client.available())
            {                           // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // limit the size of the stored received HTTP request
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1))
                {
                    HTTP_req[req_index] = c; // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank)
                {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    // Se cargan todos los archivos previos a la pagina web
                    if (StrContains(HTTP_req, "ajax_inputs"))
                    {
                        // send rest of HTTP header
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();
                        SetLEDs();
                        // envio el contenido de los inputs
                        XML_response(client);
                    }
                    else if (StrContains(HTTP_req, "css/estilos.css"))
                    {
                        client.println("Content-Type: text/css");
                        client.println("Connection: keep-alive");
                        client.println();
                        // envio de estilos personalizados
                        webFile = SD.open("css/estilos.css"); // open css file
                        if (webFile)
                        {
                            while (webFile.available())
                            {
                                client.write(webFile.read()); // send css to client
                            }
                            webFile.close();
                        }
                    }
                    else if (StrContains(HTTP_req, "logTemp.txt"))
                    {
                        client.println("Content-Type: text/plain");
                        client.println("Connection: keep-alive");
                        client.println();
                        // envio de estilos personalizados
                        webFile = SD.open("logTemp.txt"); // open css file
                        if (webFile)
                        {
                            while (webFile.available())
                            {
                                client.write(webFile.read()); // send css to client
                            }
                            webFile.close();
                        }
                    }

                    /*/
                    else if (StrContains(HTTP_req, "GET /img/focooff.png"))
                    {
                        Serial.println("img/focooff.png");
                        webFile = SD.open("img/focooff.png");
                        delay(100);
                        if (webFile)
                        {
                            client.println();

                            byte clientBuf[64];
                            int clientCount = 0;

                            while (webFile.available())
                            {
                                clientBuf[clientCount] = webFile.read();
                                clientCount++;

                                if (clientCount > 63)
                                {
                                    // Serial.println("Packet");
                                    client.write(clientBuf, 64);
                                    clientCount = 0;
                                }
                            }
                            //final <64 byte cleanup packet
                            if (clientCount > 0)
                                client.write(clientBuf, clientCount);
                            // close the file:
                            webFile.close();
                            Serial.println("close png");
                        }
                        delay(1);
                    }
                    */
                    else
                    { // web page request
                        // send rest of HTTP header
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        // send web page
                        webFile = SD.open("index.htm"); // open web page file
                        if (webFile)
                        {
                            while (webFile.available())
                            {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }
                    // display received HTTP request on serial port
                    //Serial.print(HTTP_req);
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n')
                {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                }
                else if (c != '\r')
                {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            }          // end if (client.available())
        }              // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    }                  // end if (client)
}
// checks if received HTTP request is switching on/off LEDs
// also saves the state of the LEDs
void SetLEDs(void)
{
    // MODO AUTOMATICO O MANUAL
    if (StrContains(HTTP_req, "AUTOMANUAL1=1"))
    {
        Serial.println("dentro d if StrContains automanual1=1");
        AUTOMANUAL_state[0] = 1;
    }
    else if (StrContains(HTTP_req, "AUTOMANUAL1=0"))
    {
        Serial.println("dentro d else StrContains automanual1=0");
        AUTOMANUAL_state[0] = 0;
    }

    // MODO 12 HORAS ARMARIO GRANDE
    if (StrContains(HTTP_req, "MODOAG1=1"))
    {
        MODOAG_state[0] = 1; //el modo 12 horas esta activado
        MODOAG_state[1] = 0; // el modo 18 horas desactivado
                             // EEPROM.write(0, 1);
    }
    else if (StrContains(HTTP_req, "MODOAG1=0"))
    {
        MODOAG_state[0] = 0; // el modo 12 horas desactivado
                             //  EEPROM.write(0, 0);
    }
    // MODO 12 HORAS ARMARIO GRANDE
    if (StrContains(HTTP_req, "MODOAG2=1"))
    {
        MODOAG_state[1] = 2; //el modo 18 horas esta activado
        MODOAG_state[0] = 0; // el modo 12 horas desactivado
                             // EEPROM.write(0, 2);
    }
    else if (StrContains(HTTP_req, "MODOAG2=0"))
    {
        MODOAG_state[1] = 0; // el modo 18 horas desactivado
                             //  EEPROM.write(0, 0);
    }

    // LED 1 (pin 8)
    if (StrContains(HTTP_req, "LED1=1"))
    {
        LED_state[0] = 1; // save LED state
        digitalWrite(8, HIGH);
    }
    else if (StrContains(HTTP_req, "LED1=0"))
    {
        LED_state[0] = 0; // save LED state
        digitalWrite(8, LOW);
    }
    // LED 2 (pin 9)
    if (StrContains(HTTP_req, "LED2=1"))
    {
        LED_state[1] = 1; // save LED state
        digitalWrite(9, HIGH);
    }
    else if (StrContains(HTTP_req, "LED2=0"))
    {
        LED_state[1] = 0; // save LED state
        digitalWrite(9, LOW);
    }
}

// send the XML file with analog values, switch status and LED status
void XML_response(EthernetClient cl)
{
    int t1, h1, t2, h2;

    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");

    //envio la hora y la fecha del arduino.
    dt = clock.getDateTime();
    cl.print("<relog>");
    if (dt.hour < 10)
    {
        cl.print("0");
    }
    cl.print(dt.hour);
    cl.print(":");
    if (dt.minute < 10)
    {
        cl.print("0");
    }
    cl.print(dt.minute);
    cl.print(":");
    if (dt.second < 10)
    {
        cl.print("0");
    }
    cl.print(dt.second);

    cl.print("  Fecha: ");
    cl.print(dt.day);
    cl.print("/");
    cl.print(dt.month);
    cl.print("/");
    cl.print(dt.year);
    cl.println("</relog>");

    //AG izquierda dht11 conectado al pin digital 2
    if (isnan(dht1.readTemperature()))
    {
        Serial.println("el dht1 no esta conectado o falla");
        cl.print("<digital2>");
        cl.print("dht1 no conectado");

        cl.println("</digital2>");
    }
    else
    {

        Serial.println("Estoy en el if de sensor conectado");
        t1 = dht1.readTemperature();
        h1 = dht1.readHumidity();
        cl.print("<digital2>");
        cl.print(t1);
        cl.print("°C - H: ");
        cl.print(h1);
        cl.print("%");
        cl.println("</digital2>");
    }
    //AG derecha dht11 conectado al pin digital 3
    if (isnan(dht2.readTemperature()))
    {
        Serial.println("el dht2 no esta conectado o falla");
        cl.print("<digital3>");
        cl.print("dht1 no conectado");

        cl.println("</digital3>");
    }
    else
    {

        Serial.println("Estoy en el if de sensor conectado");
        t2 = dht2.readTemperature();
        h2 = dht2.readHumidity();
        cl.print("<digital3>");
        cl.print(t2);
        cl.print("°C - H: ");
        cl.print(h2);
        cl.print("%");
        cl.println("</digital3>");
    }

    // button MODO AUTO O MANUAL armario grande
    cl.print("<AUTOMANUAL>");
    Serial.println("automanual_state[0]");
    Serial.println(AUTOMANUAL_state[0]);
    if (AUTOMANUAL_state[0])
    {
        cl.print("on");
    }
    else
    {
        cl.print("off");
    }
    cl.println("</AUTOMANUAL>");

    // button MODOAG 12 horas armario grande
    cl.print("<MODOAG>");
    // Serial.println("ModoAG_state[0");
    // Serial.println(MODOAG_state[0]);
    if (MODOAG_state[0])
    {
        cl.print("on");
    }
    else
    {
        cl.print("off");
    }
    cl.println("</MODOAG>");

    // button MODOAG 18 horas armario grande
    cl.print("<MODOAG>");
    //  Serial.println("ModoAG_state[1");
    // Serial.println(MODOAG_state[1]);
    if (MODOAG_state[1])
    {
        cl.print("on");
    }
    else
    {
        cl.print("off");
    }
    cl.println("</MODOAG>");

    // button LED states
    // LED1
    cl.print("<LED>");
    if (LED_state[0])
    {
        cl.print("on");
    }
    else
    {
        cl.print("off");
    }
    cl.println("</LED>");
    // LED2
    cl.print("<LED>");
    if (LED_state[1])
    {
        cl.print("on");
    }
    else
    {
        cl.print("off");
    }
    cl.println("</LED>");

    cl.print("</inputs>");
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++)
    {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);

    if (strlen(sfind) > len)
    {
        return 0;
    }
    while (index < len)
    {
        if (str[index] == sfind[found])
        {
            found++;
            if (strlen(sfind) == found)
            {
                return 1;
            }
        }
        else
        {
            found = 0;
        }
        index++;
    }

    return 0;
}

void ag_modo12h()
{

    if (millis() > TiempoAhora1 + periodo)
    {
        TiempoAhora1 = millis();
        Serial.println("EJECUTANDO MODO12H");
        dt = clock.getDateTime();
        if (dt.minute == 21 || dt.hour == 22 || dt.hour == 23 || dt.hour == 00 || dt.hour == 1 || dt.hour == 2 || dt.hour == 3 || dt.hour == 4 || dt.hour == 5 || dt.hour == 6 || dt.hour == 7 || dt.hour == 8)
        {
            //foco1 = lado izquierdo del armario
            LED_state[0] = 1; // save LED state
            digitalWrite(8, HIGH);
            //foco 2 = lado derecho del armario
            LED_state[1] = 1; // save LED state
            digitalWrite(9, HIGH);
        }
        else
        {
            //foco1 = lado izquierdo del armario
            LED_state[0] = 0; // save LED state
            digitalWrite(8, LOW);
            //foco 2 = lado derecho del armario
            LED_state[1] = 0; // save LED state
            digitalWrite(9, LOW);
        }
    }
}

void ag_modo18h()
{
    if (millis() > TiempoAhora2 + periodo)
    {
        TiempoAhora2 = millis();
        Serial.println("EJECUTANDO MODO18H");
        dt = clock.getDateTime();
        if (dt.minute == 33 || dt.hour == 20 || dt.hour == 21 || dt.hour == 22 || dt.hour == 23 || dt.hour == 00 || dt.hour == 1 || dt.hour == 2 || dt.hour == 3 || dt.hour == 4 || dt.hour == 5 || dt.hour == 6 || dt.hour == 7 || dt.hour == 8 || dt.hour == 9 || dt.hour == 10 || dt.hour == 11 || dt.hour == 12)
        {
            //foco1 = lado izquierdo del armario
            LED_state[0] = 1; // save LED state
            digitalWrite(8, HIGH);
            //foco 2 = lado derecho del armario
            LED_state[1] = 1; // save LED state
            digitalWrite(9, HIGH);
        }
        else
        {
            //foco1 = lado izquierdo del armario
            LED_state[0] = 0; // save LED state
            digitalWrite(8, LOW);
            //foco 2 = lado derecho del armario
            LED_state[1] = 0; // save LED state
            digitalWrite(9, LOW);
        }
    }
}

//armario grande
void control_AG()
{
    if (millis() > TiempoAhora3 + periodo)
    {
        TiempoAhora3 = millis();

        Serial.println("CONTROL AG -  estoy controlando el encendido del armario GRANDE");

        // estadoAgModoLuz = EEPROM.read(0);

        Serial.println("var MODOAG_STATE[0]:  ");
        Serial.println(MODOAG_state[0]);
        Serial.println("var MODOAG_STATE[1]:  ");
        Serial.println(MODOAG_state[1]);

        if (MODOAG_state[0] == 1)
        {
            ag_modo12h();
        }
        else if (MODOAG_state[1] == 1)
        {
            ag_modo18h();
        }
    }
}

//Guarda un registro de la temperatura del armario grande
void registroDatos()
{
    if (millis() > TiempoAhora4 + 60000)
    {
        TiempoAhora4 = millis();
        Serial.println("RegistroDatos");

        String dataString = "";
        //abro el archivo
        File dataFile = SD.open("logTemp.txt", FILE_WRITE);

        int t1, h1, t2, h2;

        dt = clock.getDateTime();
        t1 = dht1.readTemperature();
        h1 = dht1.readHumidity();
        t2 = dht2.readTemperature();
        h2 = dht2.readHumidity();

        dataString += String(dt.day) + String(":") + String(dt.month) + String(":") + String(dt.year) + String(",") +
                      String(dt.hour) + String(":") + String(dt.minute) + String(",") +
                      String("T1: ") + String(t1) + String(" C") + String(",") + String(h1) + String(" H") + String(",") +
                      String("T2: ") + String(t2) + String(" C") + String(",") + String(h2) + String(" H");
        if (dataFile)
        {
            dataFile.print((millis() / 60000));
            dataFile.print(", ");
            dataFile.println(dataString);
            dataFile.close();
            Serial.println("dataString: ");
            Serial.println(dataString);
        }
        else
        {
            Serial.println("error al abrir logTemp.txt");
        }
    }
}
