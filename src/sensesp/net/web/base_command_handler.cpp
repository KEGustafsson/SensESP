#include "base_command_handler.h"

#include "sensesp/net/web/autogen/frontend_files.h"
#include "sensesp/ui/status_page_item.h"

namespace sensesp {

void add_http_reset_handler(std::shared_ptr<HTTPServer>& server) {
  auto reset_handler = std::make_shared<HTTPRequestHandler>(
      1 << HTTP_POST, "/api/device/reset", [](httpd_req_t* req) {
        httpd_resp_send(req,
                        "Resetting device back to factory defaults. "
                        "You may have to reconfigure the WiFi settings.",
                        0);
        event_loop()->onDelay(500, []() { SensESPBaseApp::get()->reset(); });
        return ESP_OK;
      });
  server->add_handler(reset_handler);
}

void add_http_restart_handler(std::shared_ptr<HTTPServer>& server) {
  auto restart_handler = std::make_shared<HTTPRequestHandler>(
      1 << HTTP_POST, "/api/device/restart", [](httpd_req_t* req) {
        httpd_resp_send(req, "Restarting device", 0);
        event_loop()->onDelay(500, []() { ESP.restart(); });
        return ESP_OK;
      });
  server->add_handler(restart_handler);
}

void add_http_info_handler(std::shared_ptr<HTTPServer>& server) {
  auto info_handler = std::make_shared<HTTPRequestHandler>(
      1 << HTTP_GET, "/api/info", [](httpd_req_t* req) {
        auto status_page_items = StatusPageItemBase::get_status_page_items();

        JsonDocument json_doc;
        JsonArray info_items = json_doc.to<JsonArray>();

        for (auto info_item = status_page_items->begin();
             info_item != status_page_items->end(); ++info_item) {
          info_items.add(info_item->second->as_json());
        }

        String response;
        serializeJson(json_doc, response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, response.c_str());
        return ESP_OK;
      });
  server->add_handler(info_handler);
}

void add_routes_handlers(std::shared_ptr<HTTPServer>& server) {
  std::vector<RouteDefinition> routes;

  routes.push_back(RouteDefinition("Status", "/status", "StatusPage"));
  routes.push_back(RouteDefinition("System", "/system", "SystemPage"));
  routes.push_back(RouteDefinition("WiFi", "/wifi", "WiFiConfigPage"));
  routes.push_back(RouteDefinition("Signal K", "/signalk", "SignalKPage"));
  routes.push_back(
      RouteDefinition("Configuration", "/configuration", "ConfigurationPage"));

  // Pre-render the response
  JsonDocument json_doc;
  JsonArray routes_json = json_doc.to<JsonArray>();

  int sz = routes.size();

  for (auto it = routes.begin(); it != routes.end(); ++it) {
    routes_json.add(it->as_json());
  }

  String response;

  serializeJson(routes_json, response);

  auto routes_handler = std::make_shared<HTTPRequestHandler>(
      1 << HTTP_GET, "/api/routes", [response](httpd_req_t* req) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, response.c_str());
        return ESP_OK;
      });
  server->add_handler(routes_handler);

  // Find the root page

  StaticFileData* root_page = nullptr;
  for (int i = 0; i < sizeof(kFrontendFiles) / sizeof(StaticFileData); i++) {
    if (strcmp(kFrontendFiles[i].url, "/") == 0) {
      root_page = (StaticFileData*)&kFrontendFiles[i];
      break;
    }
  }
  if (root_page == nullptr) {
    ESP_LOGE(__FILENAME__, "Root page not found in kWebUIFiles");
    return;
  }

  // Add a handler for each route that returns the root page

  for (auto it = routes.begin(); it != routes.end(); ++it) {
    String path = it->get_path();
    auto route_handler = std::make_shared<HTTPRequestHandler>(
        1 << HTTP_GET, path.c_str(), [root_page](httpd_req_t* req) {
          httpd_resp_set_type(req, root_page->content_type);
          if (root_page->content_encoding != nullptr) {
            httpd_resp_set_hdr(req, kContentEncoding,
                               root_page->content_encoding);
          }
          httpd_resp_send(req, root_page->content, root_page->content_length);
          return ESP_OK;
        });
    server->add_handler(route_handler);
  }
}

void add_base_app_http_command_handlers(std::shared_ptr<HTTPServer>& server) {
  add_http_reset_handler(server);
  add_http_restart_handler(server);
  add_http_info_handler(server);
  add_routes_handlers(server);
}

}  // namespace sensesp
