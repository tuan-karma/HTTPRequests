/*
Simple HTTP Requests Library for esp32/esp8266.
Just a wrapper of Client.h class to make https GET, POST request more conveniently.
*/

#pragma once

#include <Arduino.h>

#include <Client.h>

// #include <WiFiClient.h>
// #include <WiFiClientSecure.h>

enum class HTTP_CODES
{
    HTTP_OK,
    CONN_FAILED,
    SEND_FAILED,
    STATUS_FAILED,
    HEADER_FAILED,
    INVALID_URL,
};

class HTTPR
{
public:
    HTTPR(Client &client, uint16_t port = 443, const char *httpver = "1.1");
    ~HTTPR();

    HTTP_CODES get(const char *url);
    HTTP_CODES post(const char *url);

    void dump_client();

    // fn(const char* url) --> const char* host, port, path
    bool parse_url(const char *url);

    uint16_t get_port() {return _port;}
    const char* get_host() {return _host;}
    const char* get_path() {return _path;}

private:
    Client &_client;
    const char *_version = "1.1"; // the HTTP version

    char _url[512] = "www.example.com/";
    uint16_t _port = 443;
    const char *_host;
    const char *_path; // Note: need to pad '/' before this to form a legal url's path
};

/***** helpers ******/
void HTTPR::dump_client()
{
    while (_client.connected())
    {
        while (_client.available())
        {
            char c = _client.read();
            Serial.write(c);
        }
        yield();
    }
}

// fn(const char* url) --> const char* host, port, path
// url pattern: <scheme:http_or_https>://<host:www.example.com>:<port_optional>/<path:the_remain>
// https://developer.mozilla.org/en-US/docs/Learn/Common_questions/What_is_a_URL 
bool HTTPR::parse_url(const char *url)
{
    bool success = true;
    strlcpy(_url, url, sizeof(_url));

    const char *current_pos = _url;
    // Guess the `port` number from the http/https prefix in url:
    if (strncmp(url, "https", 5) == 0)
    {
        _port = 443;
        current_pos = _url + 8; // increment the pointer to pass over "https://"
        _host = current_pos;
    }
    else if (strncmp(url, "http", 4) == 0)
    {
        _port = 80;
        current_pos = _url + 7; // increment the pointer to pass over "http://"
        _host = current_pos;
    }

    char *colon = strchr(current_pos, ':'); // check the `:<port_number>` in _url[]
    if (colon != NULL) // found ':'
    {
        *colon = '\0'; // replace ':' --> '\0' --> mark null-terminal at the end of host_name
        current_pos = colon + 1;
    }

    char *slash = strchr(current_pos, '/');
    if (slash != NULL) // found '/' after the <port_number>
    {
        *slash = '\0';
        _path = slash + 1;
    }
    else // slash not found --> path not found
    {
        _path = nullptr; 
        success = false;
    }

    _host = url;
    
    if (colon != NULL) 
    {
        _port = atoi(colon + 1);
        if (_port == 0) success = false;  // invalid url (contains `:<not_a_number` pattern)
    }
    
    return success;
}
/*-------------------*/

HTTPR::HTTPR(Client &client, uint16_t port, const char *http_version)
    : _client(client), _port(port), _version(http_version)
{
}

HTTPR::~HTTPR()
{
    _client.stop();
}

HTTP_CODES HTTPR::get(const char *url)
{
    if (parse_url(url) == false)
    {
        log_i("Invalid URL");
        return HTTP_CODES::INVALID_URL;
    }
    // Try to connect
    if (!_client.connect(url, _port))
    {
        log_i("Connection failed!");
        return HTTP_CODES::CONN_FAILED;
    }
    yield();

    // Make a HTTP request:
    _client.printf("GET /%s HTTP/%s\n", _path, _version);
    _client.printf("Host: %s\n", _host);
    _client.println("User-Agent: ESP32");
    _client.println("Connection: close");
    _client.println("Cache-Control: no-cache");
    if (_client.println() == 0)
    {
        log_i("Failed to send request");
        _client.stop();
        return HTTP_CODES::SEND_FAILED;
    }

    // Check HTTP status
    char status[32] = {0};
    _client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status + 9, "200 OK") != 0)
    {
        Serial.print("Unexptected HTTP status: ");
        Serial.println(status);
        _client.stop();
        return HTTP_CODES::STATUS_FAILED;
    }

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!_client.find(endOfHeaders))
    {
        Serial.println("Invalid header");
        _client.stop();
        return HTTP_CODES::HEADER_FAILED;
    }
    return HTTP_CODES::HTTP_OK;
}