#include "http-parser.h"
#include "logger.h"

using namespace http_parser;

namespace http_parser_test {

  void TestGET() {
    string data =
      "GET / HTTP/1.1\n"
      "Host: localhost:12345\n"
      "Connection: keep-alive\n"
      "Accept: application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\n"
      "User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_8; en-US) AppleWebKit/534.7 (KHTML, like Gecko) Chrome/7.0.517.41 Safari/534.7\n"
      "Accept-Encoding: gzip,deflate,sdch\n"
      "Accept-Language: en-US,en;q=0.8\n"
      "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n"
      "\n";
    HttpParser parser;
    parser.Parse(data.c_str(), data.length());
    const HttpRequest& request = parser.GetHttpRequest();
    log::Debug("Found request! type=%s path=%s",
	       request.GetRequestType(),
	       request.GetPath().c_str());
  }

  void TestSplitUpGET() {
    string incomplete_data =
      "GET / HTTP/1.1\n"
      "Host: localhost:12345\n"
      "Connection: keep-alive\n"
      "Accept: application/xml,application/xhtml+xml,text/ht";

    string rest_of_data =
      "ml;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\n"
      "User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_8; en-US) AppleWebKit/534.7 (KHTML, like Gecko) Chrome/7.0.517.41 Safari/534.7\n"
      "Accept-Encoding: gzip,deflate,sdch\n"
      "Accept-Language: en-US,en;q=0.8\n"
      "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n"
      "\n";

    HttpParser parser;
    parser.Parse(incomplete_data.c_str(), incomplete_data.length());
    parser.Parse(rest_of_data.c_str(), rest_of_data.length());
    const HttpRequest& request = parser.GetHttpRequest();
    log::Debug("Found request! type=%s path=%s",
	       request.GetRequestType(),
	       request.GetPath().c_str());
  }

  void TestIncompleteGET() {
    HttpParser parser;
    string incomplete_data = "GET path ";
    string rest_of_data = "HTT\n\n";
    bool result = parser.Parse(incomplete_data.c_str(), incomplete_data.length());
    if (result) {
      log::Error("Error! Should require more data!");
      exit(-1);
    }
    result = parser.Parse(rest_of_data.c_str(), rest_of_data.length());
    if (result) {
      log::Error("Error! Should require more data!");
      exit(-1);
    }
  }
}

int main() {
  log::Debug("Testing HttpParser...");

  // http_parser_test::TestGET();
  // http_parser_test::TestSplitUpGET();
  http_parser_test::TestIncompleteGET();
}
