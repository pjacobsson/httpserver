#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>

using namespace std;

// TODO: Parser only gets the request method and path. Needs to extract
// everything.
namespace http_parser {

  class HttpRequest {
  public:
    enum request_type { UNSET, GET, POST, PUT, DELETE };

    HttpRequest();

    const request_type GetRequestType() const;
    void SetRequestType(const string& request_type);

    const string& GetPath() const;
    void SetPath(const string& path);

  private:
    request_type request_type_;
    string path_;

    HttpRequest(const HttpRequest&);
    HttpRequest& operator=(const HttpRequest&);
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
		 HEADER_VALUE,
		 FORM_DATA };

    state current_state_;
    string field_;

    string last_header_key_;
    int content_length_;
    int collected_content_length_;

    HttpRequest request_;

    HttpParser(const HttpParser&);
    HttpParser& operator=(const HttpParser&);
  };

}  // namespace http_parser

#endif  // HTTP_PARSER_H_
