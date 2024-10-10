//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                       _              //
//               _    _       _      _        _     _   _   _    _   _   _        _   _  _   _          //
//           |  | |  |_| |\| |_| |\ |_|   |\ |_|   |_| |_| | |  |   |_| |_| |\/| |_| |  |_| | |   /|    //    
//         |_|  |_|  |\  | | | | |/ | |   |/ | |   |   |\  |_|  |_| |\  | | |  | | | |_ | | |_|   _|_   //
//                                                                                       /              //
//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Área de inclusão das bibliotecas
//-----------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ioplaca.h"   // Controles das Entradas e Saídas digitais e do teclado
#include "lcdvia595.h" // Controles do Display LCD
#include "hcf_adc.h"   // Controles do ADC
#include "MP_hcf.h"   // Controles do Motor
#include "connect.h"    // Controle do Wifi
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_netif.h"
#include "mqtt_client.h"
//#include "protocol_examples_common.h"
#include "esp_system.h"
#include "nvs_flash.h"


// Área das macros
//-----------------------------------------------------------------------------------------------------------------------
# define PWD 1234

#define W_DEVICE_ID "65774aa82623fd911ab650c1" //Use o DeviceID no Wegnology  
#define W_ACCESS_KEY "76ac5ed2-ed18-4e96-9e02-d2dd572db083" //use a chave de acesso e a senha
#define W_PASSWORD "f52797619b7205bc2ac8d796d80fd0cb23f988e882cd0b82d575b26939f78c1c"
#define W_TOPICO_PUBLICAR "wnology/65774aa82623fd911ab650c1/state" //esse número no meio do tópico deve ser mudado pelo ID do seu device Wegnology
#define W_TOPICO_SUBSCREVER "wnology/65774aa82623fd911ab650c1/command" // aqui também
#define W_BROKER "mqtt://broker.app.wnology.io:1883"
#define SSID "coqueiro"
#define PASSWORD "amigos12"
// Área de declaração de variáveis e protótipos de funções
//-----------------------------------------------------------------------------------------------------------------------

static const char *TAG = "Placa";
static uint8_t entradas, saidas = 0; //variáveis de controle de entradas e saídas
int controle = 0;
int senha = 0;
int tempo = 50;
int coluna = 0;
uint32_t adcvalor = 0;
char exibir [40];
char mensa [40];
const char *strLED = "LED\":"; 
const char *subtopico_temp = "{\"data\": {\"Temperatura\": ";
char * Inform;
bool ledstatus;
esp_mqtt_client_handle_t cliente;
int temperatura = 0;
// Funções e ramos auxiliares para o cofre
//-----------------------------------------------------------------------------------------------------------------------


// Funções e ramos auxiliares para o IoT
//-----------------------------------------------------------------------------------------------------------------------

void time_sync_notification_cb(struct timeval *tv) 
{
    printf("Hora sincronizada!\n");
}

// Função para configurar o NTP e sincronizar o tempo

void initialize_sntp(void)
 {
    printf("Inicializando o SNTP...\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");  // Servidor NTP
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}
// Função para obter a hora e o dia da semana
void obtain_time(void) 
{
    time_t now;
    struct tm timeinfo;

    // Espera até que o tempo seja sincronizado
    time(&now);
    localtime_r(&now, &timeinfo);
    
       // Ajusta a hora para o horário de Brasília (UTC-3)
    timeinfo.tm_hour += -3;  // Ajuste para UTC-3

    // Normaliza a estrutura de tempo
    mktime(&timeinfo);

    while (timeinfo.tm_year < (2016 - 1900)) 
    {
        printf("Aguardando sincronização do tempo...\n");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    // Exibe a hora e o dia da semana no formato desejado
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%c", &timeinfo);  // Formata data e hora
    printf("Hora atual: %s\n", buffer);

    // Obtém o dia da semana (0 = Domingo, 6 = Sábado)
    int weekday = timeinfo.tm_wday;
    printf("Dia da semana: %d\n", weekday);
}

// Programa Principal
//-----------------------------------------------------------------------------------------------------------------------

void app_main(void)
{
    /////////////////////////////////////////////////////////////////////////////////////   Programa principal



// a seguir, apenas informações de console, aquelas notas verdes no início da execução
    ESP_LOGI(TAG, "Iniciando...");
    ESP_LOGI(TAG, "Versão do IDF: %s", esp_get_idf_version());

    // Inicializações de periféricos (manter assim)
    ESP_ERROR_CHECK(nvs_flash_init());

    // inicializar os IOs e teclado da placa
    ioinit();      
    entradas = io_le_escreve(saidas); // Limpa as saídas e lê o estado das entradas

    // inicializar o display LCD 
    lcd595_init();
    lcd595_write(1,0,"COFRE IOT - v1.0");
    vTaskDelay(1000 / portTICK_PERIOD_MS);  

    lcd595_write(1,0,"Inicializando   ");
    lcd595_write(2,0,"ADC             ");

    // Inicializar o componente de leitura de entrada analógica
    esp_err_t init_result = hcf_adc_iniciar();
    if (init_result != ESP_OK) 
    {
        ESP_LOGE("MAIN", "Erro ao inicializar o componente ADC personalizado");
    }

    lcd595_write(2,0,"ADC / Wifi      ");
    lcd595_write(1,13,".  ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 

    // Inicializar a comunicação IoT
    wifi_init();    
    ESP_ERROR_CHECK(wifi_connect_sta(SSID, PASSWORD, 10000));

    lcd595_write(2,0,"C / Wifi / MQTT ");
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"Wifi / MQTT     ");
    lcd595_write(1,13,".. ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    sprintf(mensa,"%s %d }}",subtopico_temp,temperatura);
    //mqtt_app_start();

    // inicializa driver de motor de passo com fins de curso nas entradas 6 e 7 da placa
    lcd595_write(2,0,"i / MQTT / DRV  ");
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"MQTT / DRV      ");
    lcd595_write(1,13,"...");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    DRV_init(6,7);

    // fecha a tampa, se estiver aberta
    hcf_adc_ler(&adcvalor);                
    adcvalor = adcvalor*360/4095;
    // if(adcvalor>50) fechar();

    lcd595_write(2,0,"TT / DRV / APP  ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"DRV / APP       ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"V / APP         ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"APP             ");

     for(int i = 0; i < 10; i++) 
     {
        lcd595_write(1,13,"   ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,".  ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,".. ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,"...");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
    }
    
    lcd595_clear();

    // Inicializa o SNTP para obter a hora da internet
    initialize_sntp();

    // Espera alguns segundos para sincronização
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Obtém e imprime a hora e o dia da semana uma vez na inicialização
    obtain_time();

    
    /////////////////////////////////////////////////////////////////////////////////////   Periféricos inicializados


    /////////////////////////////////////////////////////////////////////////////////////   Início do ramo principal                    
    while (1)                                                                                                                         
    {                                                                                                                                 
        //_______________________________________________________________________________________________________________________________________________________ //
        //-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - -  -  -  -  -  -  -  -  -  -  Escreva seu código aqui!!! //
             
                                                                                                          
    vTaskDelay(60000 / portTICK_PERIOD_MS);  // Executa a cada minuto
        obtain_time();  // Atualiza a hora e imprime novamente

        
        //-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - -  -  -  -  -  -  -  -  -  -  Escreva seu só até aqui!!! //
        //________________________________________________________________________________________________________________________________________________________//
        vTaskDelay(200 / portTICK_PERIOD_MS);    // delay mínimo obrigatório, se retirar, pode causar reset do ESP
    }
    
    // caso erro no programa, desliga o módulo ADC
    hcf_adc_limpar();

    /////////////////////////////////////////////////////////////////////////////////////   Fim do ramo principal
}