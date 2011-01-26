#include <iostream>
#include <string>

#include "http-parser.h"
#include "logger.h"

using namespace std;

// TODO: Way too much copying of strings going on here?

namespace http_parser {

  HttpRequest::HttpRequest() {}

  const HttpRequest::request_type HttpRequest::GetRequestType() const {
    return request_type_;
  }

  void HttpRequest::SetRequestType(const string& request_type) {
    if (request_type == "GET") {
      request_type_ = GET;
    }
  }

  const string& HttpRequest::GetPath() const {
    return path_;
  }

  void HttpRequest::SetPath(const string& path) {
    path_ = path;
  }

  HttpParser::HttpParser(): current_state_(REQUEST_TYPE), field_("") {}

  bool HttpParser::Parse(const char data[], int length) {
    int index = 0;
    bool done = false;
    bool first_in_line;
    char current_char = '\n';

    while (!done) {
      first_in_line = (current_char == '\n');
      current_char = data[index];
      if (first_in_line &&
	  current_char == '\r') {
	current_char = data[++index]; // assume current_char == '\n' now
	log::Debug("Blank line");
	break;
      }

      switch (current_state_) {
      case REQUEST_TYPE:
	if (current_char == ' ') {
	  log::Debug("Request [%s]", field_.c_str());
	  request_.SetRequestType(field_);
	  field_ = "";
	  current_state_ = PATH;
	} else {
	  field_.append(1, current_char);
	}
	break;
      case PATH:
	if (current_char == ' ') {
          log::Debug("Path [%s]", field_.c_str());
	  request_.SetPath(field_);
	  field_ = "";
	  current_state_ = PROTOCOL;
	} else {
	  field_.append(1, current_char);
	}
	break;
      case PROTOCOL:
	if (current_char == '\n') {
	  log::Debug("Protocol [%s]", field_.c_str());
	  field_ = "";
	  current_state_ = HEADER_KEY;
	} else {
	  field_.append(1, current_char);
	}
	break;
      case HEADER_KEY:
	if (current_char == ':') {
	  log::Debug("Header key [%s]", field_.c_str());
	  field_ = "";
	  current_state_ = HEADER_DELIM;
	} else {
	  field_.append(1, current_char);
	}
	break;
      case HEADER_DELIM:
	log::Debug("Header delim [%d]", current_char);
	if (current_char == ' ') {
	  field_ = "";
	  current_state_ = HEADER_VALUE;
	} else {
	  log::Error("Unexpected header delim!");
	}
	break;
      case HEADER_VALUE:
	if (current_char == '\r') {
	  current_char = data[++index]; // assume current_char == '\n' now
	  log::Debug("Header value [%s]", field_.c_str());
	  field_ = "";
	  current_state_ = HEADER_KEY;
	} else {
	  field_.append(1, current_char);
	}
	break;
      }
      index++;
      if (index == length) {
	log::Debug("Needs more data");
	return false;
      }
    }
    return true;
  }

  const HttpRequest& HttpParser::GetHttpRequest() {
    return request_;
  }

}
