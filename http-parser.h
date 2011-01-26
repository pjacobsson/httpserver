#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>

using namespace std;

namespace http_parser {

  class HttpRequest {
  public:
    enum request_type { GET, POST, PUT, DELETE };

    HttpRequest();

    const request_type GetRequestType() const;
    void SetRequestType(const string& request_type);

    const string& GetPath() const;
    void SetPath(const string& path);

  private:
    HttpRequest(const HttpRequest&);
    HttpRequest& operator=(const HttpRequest&);

    request_type request_type_;
    string path_;
  };

  class HttpParser {
  public:
    HttpParser();
    bool Parse(const char data[], int length);
    const HttpRequest& GetHttpRequest();
  private:
    enum state { REQUEST_TYPE,
		 PATH,
		 PROTOCOL,
		 HEADER_KEY,
		 HEADER_DELIM,
		 HEADER_VALUE };

    int index;
    state current_state;
    string field;
    HttpRequest request;
  };

}  // namespace http_parser

#endif  // HTTP_PARSER_H_
