//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ESP32-CAM_Camera_Test
/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/
  
  IMPORTANT!!! 
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

/*
 * Uteh Str
 * 
 * This project is from : https://github.com/peyanski/ESP32-CAM/blob/master/ESP32-cam-simple-web-server.ino 
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //--> disable brownout problems
#include "soc/rtc_cntl_reg.h"  //--> disable brownout problems
#include "esp_http_server.h"

// Replace with your network credentials
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"

// This project was tested with the AI Thinker Model, M5STACK PSRAM Model and M5STACK WITHOUT PSRAM
#define CAMERA_MODEL_AI_THINKER
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM

// Not tested with this model
//#define CAMERA_MODEL_WROVER_KIT

#if defined(CAMERA_MODEL_WROVER_KIT)
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM    21
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  
  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       32
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_M5STACK_WITHOUT_PSRAM)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    15
  #define XCLK_GPIO_NUM     27
  #define SIOD_GPIO_NUM     25
  #define SIOC_GPIO_NUM     23
  
  #define Y9_GPIO_NUM       19
  #define Y8_GPIO_NUM       36
  #define Y7_GPIO_NUM       18
  #define Y6_GPIO_NUM       39
  #define Y5_GPIO_NUM        5
  #define Y4_GPIO_NUM       34
  #define Y3_GPIO_NUM       35
  #define Y2_GPIO_NUM       17
  #define VSYNC_GPIO_NUM    22
  #define HREF_GPIO_NUM     26
  #define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Camera model not selected"
#endif

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    bool detected = false;
    int face_id = 0;
    if(true){
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
  };

  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &index_uri);
  }
  
  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.println();
  Serial.printf("Camera Capture Ready! Go to: http://%s:%d%s\n", WiFi.localIP().toString().c_str(), config.server_port, capture_uri.uri);
  Serial.printf("Starting snapshot server on port: '%d'\n", config.server_port);
  Serial.println();
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(camera_httpd, &capture_uri);
  }
}

void setup() {
  // Disable brownout detector.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
 
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();

  Serial.println();
  Serial.print("Set the camera ESP32 CAM...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA; //UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println();
    Serial.println("Restarting the ESP32 CAM.");
    delay(1000);
    ESP.restart();
  }

  Serial.println();
  Serial.print("Set camera ESP32 CAM successfully.");
  
  // Wi-Fi connection
  Serial.println();
  Serial.print("Connecting to : ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println();
  
  // Start streaming web server
  startCameraServer();
}

void loop() {
  delay(1);
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ESP32_CAM_Send_Photo_to_Google_Drive

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WARNING!!! Make sure that you have either selected Ai Thinker ESP32-CAM or ESP32 Wrover Module or another board which has PSRAM enabled.                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reference :                                                                                                                                                //
// - esp32cam-gdrive : https://github.com/gsampallo/esp32cam-gdrive                                                                                           //
// - ESP32 CAM Send Images to Google Drive, IoT Security Camera : https://www.electroniclinic.com/esp32-cam-send-images-to-google-drive-iot-security-camera/  //
// - esp32cam-google-drive : https://github.com/RobertSasak/esp32cam-google-drive                                                                             //
//                                                                                                                                                            //
// When the ESP32-CAM takes photos or the process of sending photos to Google Drive is in progress, the ESP32-CAM requires a large amount of power.           //
// So I suggest that you use a 5V power supply with a current of approximately 2A.                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//======================================== Including the libraries.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"
//======================================== 

//======================================== CAMERA_MODEL_AI_THINKER GPIO.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
//======================================== 

// LED Flash PIN (GPIO 4)
#define FLASH_LED_PIN 4

//======================================== Enter your WiFi ssid and password.
const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";
//======================================== 

//======================================== Replace with your "Deployment ID" and Folder Name.
String myDeploymentID = "REPLACE_WITH_YOUR_DEPLOYMENT_ID";
String myMainFolderName = "ESP32-CAM";
//======================================== 

//======================================== Variables for Timer/Millis.
unsigned long previousMillis = 0; 
const int Interval = 20000; //--> Capture and Send a photo every 20 seconds.
//======================================== 

// Variable to set capture photo with LED Flash.
// Set to "false", then the Flash LED will not light up when capturing a photo.
// Set to "true", then the Flash LED lights up when capturing a photo.
bool LED_Flash_ON = true;

// Initialize WiFiClientSecure.
WiFiClientSecure client;

//________________________________________________________________________________ Test_Con()
// This subroutine is to test the connection to "script.google.com".
void Test_Con() {
  const char* host = "script.google.com";
  while(1) {
    Serial.println("-----------");
    Serial.println("Connection Test...");
    Serial.println("Connect to " + String(host));
  
    client.setInsecure();
  
    if (client.connect(host, 443)) {
      Serial.println("Connection successful.");
      Serial.println("-----------");
      client.stop();
      break;
    } else {
      Serial.println("Connected to " + String(host) + " failed.");
      Serial.println("Wait a moment for reconnecting.");
      Serial.println("-----------");
      client.stop();
    }
  
    delay(1000);
  }
}
//________________________________________________________________________________ 

//________________________________________________________________________________ SendCapturedPhotos()
// Subroutine for capturing and sending photos to Google Drive.
void SendCapturedPhotos() {
  const char* host = "script.google.com";
  Serial.println();
  Serial.println("-----------");
  Serial.println("Connect to " + String(host));
  
  client.setInsecure();

  //---------------------------------------- The Flash LED blinks once to indicate connection start.
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);
  digitalWrite(FLASH_LED_PIN, LOW);
  delay(100);
  //---------------------------------------- 

  //---------------------------------------- The process of connecting, capturing and sending photos to Google Drive.
  if (client.connect(host, 443)) {
    Serial.println("Connection successful.");
    
    if (LED_Flash_ON == true) {
      digitalWrite(FLASH_LED_PIN, HIGH);
      delay(100);
    }

    //.............................. Taking a photo.
    Serial.println();
    Serial.println("Taking a photo...");
    
    for (int i = 0; i <= 3; i++) {
      camera_fb_t * fb = NULL;
      fb = esp_camera_fb_get();
       if(!fb) {
          Serial.println("Camera capture failed");
          Serial.println("Restarting the ESP32 CAM.");
          delay(1000);
          ESP.restart();
          return;
        } 
      esp_camera_fb_return(fb);
      delay(200);
    }
  
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Camera capture failed");
      Serial.println("Restarting the ESP32 CAM.");
      delay(1000);
      ESP.restart();
      return;
    } 
  
    if (LED_Flash_ON == true) digitalWrite(FLASH_LED_PIN, LOW);
    
    Serial.println("Taking a photo was successful.");
    //.............................. 

    //.............................. Sending image to Google Drive.
    Serial.println();
    Serial.println("Sending image to Google Drive.");
    Serial.println("Size: " + String(fb->len) + "byte");
    
    String url = "/macros/s/" + myDeploymentID + "/exec?folder=" + myMainFolderName;

    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Transfer-Encoding: chunked");
    client.println();

    int fbLen = fb->len;
    char *input = (char *)fb->buf;
    int chunkSize = 3 * 1000; //--> must be multiple of 3.
    int chunkBase64Size = base64_enc_len(chunkSize);
    char output[chunkBase64Size + 1];

    Serial.println();
    int chunk = 0;
    for (int i = 0; i < fbLen; i += chunkSize) {
      int l = base64_encode(output, input, min(fbLen - i, chunkSize));
      client.print(l, HEX);
      client.print("\r\n");
      client.print(output);
      client.print("\r\n");
      delay(100);
      input += chunkSize;
      Serial.print(".");
      chunk++;
      if (chunk % 50 == 0) {
        Serial.println();
      }
    }
    client.print("0\r\n");
    client.print("\r\n");

    esp_camera_fb_return(fb);
    //.............................. 

    //.............................. Waiting for response.
    Serial.println("Waiting for response.");
    long int StartTime = millis();
    while (!client.available()) {
      Serial.print(".");
      delay(100);
      if ((StartTime + 10 * 1000) < millis()) {
        Serial.println();
        Serial.println("No response.");
        break;
      }
    }
    Serial.println();
    while (client.available()) {
      Serial.print(char(client.read()));
    }
    //.............................. 

    //.............................. Flash LED blinks once as an indicator of successfully sending photos to Google Drive.
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(500);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(500);
    //.............................. 
  }
  else {
    Serial.println("Connected to " + String(host) + " failed.");
    
    //.............................. Flash LED blinks twice as a failed connection indicator.
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(500);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(500);
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(500);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(500);
    //.............................. 
  }
  //---------------------------------------- 

  Serial.println("-----------");

  client.stop();
}
//________________________________________________________________________________ 

//________________________________________________________________________________ VOID SETUP()
void setup() {
  // put your setup code here, to run once:
  
  // Disable brownout detector.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  pinMode(FLASH_LED_PIN, OUTPUT);
  
  // Setting the ESP32 WiFi to station mode.
  Serial.println();
  Serial.println("Setting the ESP32 WiFi to station mode.");
  WiFi.mode(WIFI_STA);

  //---------------------------------------- The process of connecting ESP32 CAM with WiFi Hotspot / WiFi Router.
  Serial.println();
  Serial.print("Connecting to : ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  // The process timeout of connecting ESP32 CAM with WiFi Hotspot / WiFi Router is 20 seconds.
  // If within 20 seconds the ESP32 CAM has not been successfully connected to WiFi, the ESP32 CAM will restart.
  // I made this condition because on my ESP32-CAM, there are times when it seems like it can't connect to WiFi, so it needs to be restarted to be able to connect to WiFi.
  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(250);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(250);
    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    if(connecting_process_timed_out == 0) {
      Serial.println();
      Serial.print("Failed to connect to ");
      Serial.println(ssid);
      Serial.println("Restarting the ESP32 CAM.");
      delay(1000);
      ESP.restart();
    }
  }

  digitalWrite(FLASH_LED_PIN, LOW);
  
  Serial.println();
  Serial.print("Successfully connected to ");
  Serial.println(ssid);
  //Serial.print("ESP32-CAM IP Address: ");
  //Serial.println(WiFi.localIP());
  //---------------------------------------- 

  //---------------------------------------- Set the camera ESP32 CAM.
  Serial.println();
  Serial.println("Set the camera ESP32 CAM...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 8;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println();
    Serial.println("Restarting the ESP32 CAM.");
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();

  // Selectable camera resolution details :
  // -UXGA   = 1600 x 1200 pixels
  // -SXGA   = 1280 x 1024 pixels
  // -XGA    = 1024 x 768  pixels
  // -SVGA   = 800 x 600   pixels
  // -VGA    = 640 x 480   pixels
  // -CIF    = 352 x 288   pixels
  // -QVGA   = 320 x 240   pixels
  // -HQVGA  = 240 x 160   pixels
  // -QQVGA  = 160 x 120   pixels
  s->set_framesize(s, FRAMESIZE_SXGA);  //--> UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  Serial.println("Setting the camera successfully.");
  Serial.println();

  delay(1000);

  Test_Con();

  Serial.println();
  Serial.println("ESP32-CAM captures and sends photos to the server every 20 seconds.");
  Serial.println();
  delay(2000);
}
//________________________________________________________________________________ 

//________________________________________________________________________________ VOID LOOP()
void loop() {
  // put your main code here, to run repeatedly:

  //---------------------------------------- Timer/Millis to capture and send photos to Google Drive every 20 seconds (see Interval variable).
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= Interval) {
    previousMillis = currentMillis;

    SendCapturedPhotos();
  }
  //---------------------------------------- 
}
//________________________________________________________________________________ 
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<






//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Base64.cpp
/*
 * Copyright (c) 2013 Adam Rudd.
 * See LICENSE for more information
 * https://github.com/adamvr/arduino-base64 
 */
#if (defined(__AVR__))
#include <avr\pgmspace.h>
#else
#include <pgmspace.h>
#endif

const char PROGMEM b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

/* 'Private' declarations */
inline void a3_to_a4(unsigned char * a4, unsigned char * a3);
inline void a4_to_a3(unsigned char * a3, unsigned char * a4);
inline unsigned char b64_lookup(char c);

int base64_encode(char *output, char *input, int inputLen) {
    int i = 0, j = 0;
    int encLen = 0;
    unsigned char a3[3];
    unsigned char a4[4];

    while(inputLen--) {
        a3[i++] = *(input++);
        if(i == 3) {
            a3_to_a4(a4, a3);

            for(i = 0; i < 4; i++) {
                output[encLen++] = pgm_read_byte(&b64_alphabet[a4[i]]);
            }

            i = 0;
        }
    }

    if(i) {
        for(j = i; j < 3; j++) {
            a3[j] = '\0';
        }

        a3_to_a4(a4, a3);

        for(j = 0; j < i + 1; j++) {
            output[encLen++] = pgm_read_byte(&b64_alphabet[a4[j]]);
        }

        while((i++ < 3)) {
            output[encLen++] = '=';
        }
    }
    output[encLen] = '\0';
    return encLen;
}

int base64_decode(char * output, char * input, int inputLen) {
    int i = 0, j = 0;
    int decLen = 0;
    unsigned char a3[3];
    unsigned char a4[4];


    while (inputLen--) {
        if(*input == '=') {
            break;
        }

        a4[i++] = *(input++);
        if (i == 4) {
            for (i = 0; i <4; i++) {
                a4[i] = b64_lookup(a4[i]);
            }

            a4_to_a3(a3,a4);

            for (i = 0; i < 3; i++) {
                output[decLen++] = a3[i];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) {
            a4[j] = '\0';
        }

        for (j = 0; j <4; j++) {
            a4[j] = b64_lookup(a4[j]);
        }

        a4_to_a3(a3,a4);

        for (j = 0; j < i - 1; j++) {
            output[decLen++] = a3[j];
        }
    }
    output[decLen] = '\0';
    return decLen;
}

int base64_enc_len(int plainLen) {
    int n = plainLen;
    return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

int base64_dec_len(char * input, int inputLen) {
    int i = 0;
    int numEq = 0;
    for(i = inputLen - 1; input[i] == '='; i--) {
        numEq++;
    }

    return ((6 * inputLen) / 8) - numEq;
}

inline void a3_to_a4(unsigned char * a4, unsigned char * a3) {
    a4[0] = (a3[0] & 0xfc) >> 2;
    a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
    a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
    a4[3] = (a3[2] & 0x3f);
}

inline void a4_to_a3(unsigned char * a3, unsigned char * a4) {
    a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
    a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
    a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

inline unsigned char b64_lookup(char c) {
    if(c >='A' && c <='Z') return c - 'A';
    if(c >='a' && c <='z') return c - 71;
    if(c >='0' && c <='9') return c + 4;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<





//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Base64.h
/*
 * Copyright (c) 2013 Adam Rudd.
 * See LICENSE for more information
 * https://github.com/adamvr/arduino-base64 
 */
#ifndef _BASE64_H
#define _BASE64_H

/* b64_alphabet:
 *      Description: Base64 alphabet table, a mapping between integers
 *                   and base64 digits
 *      Notes: This is an extern here but is defined in Base64.c
 */
extern const char b64_alphabet[];

/* base64_encode:
 *      Description:
 *          Encode a string of characters as base64
 *      Parameters:
 *          output: the output buffer for the encoding, stores the encoded string
 *          input: the input buffer for the encoding, stores the binary to be encoded
 *          inputLen: the length of the input buffer, in bytes
 *      Return value:
 *          Returns the length of the encoded string
 *      Requirements:
 *          1. output must not be null or empty
 *          2. input must not be null
 *          3. inputLen must be greater than or equal to 0
 */
int base64_encode(char *output, char *input, int inputLen);

/* base64_decode:
 *      Description:
 *          Decode a base64 encoded string into bytes
 *      Parameters:
 *          output: the output buffer for the decoding,
 *                  stores the decoded binary
 *          input: the input buffer for the decoding,
 *                 stores the base64 string to be decoded
 *          inputLen: the length of the input buffer, in bytes
 *      Return value:
 *          Returns the length of the decoded string
 *      Requirements:
 *          1. output must not be null or empty
 *          2. input must not be null
 *          3. inputLen must be greater than or equal to 0
 */
int base64_decode(char *output, char *input, int inputLen);

/* base64_enc_len:
 *      Description:
 *          Returns the length of a base64 encoded string whose decoded
 *          form is inputLen bytes long
 *      Parameters:
 *          inputLen: the length of the decoded string
 *      Return value:
 *          The length of a base64 encoded string whose decoded form
 *          is inputLen bytes long
 *      Requirements:
 *          None
 */
int base64_enc_len(int inputLen);

/* base64_dec_len:
 *      Description:
 *          Returns the length of the decoded form of a
 *          base64 encoded string
 *      Parameters:
 *          input: the base64 encoded string to be measured
 *          inputLen: the length of the base64 encoded string
 *      Return value:
 *          Returns the length of the decoded form of a
 *          base64 encoded string
 *      Requirements:
 *          1. input must not be null
 *          2. input must be greater than or equal to zero
 */
int base64_dec_len(char *input, int inputLen);

#endif // _BASE64_H
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<