idf_component_register(SRCS "web_server.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server mdns tmc2209 wifi_manager
                       EMBED_FILES "../web_server/html/index.html"
                                 "../web_server/html/style.css"
                                 "../web_server/html/script.js")