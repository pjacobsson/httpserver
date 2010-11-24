#include <iostream>
#include <string>

#include "logger.h"

using namespace std;

// TODO: Way too much copying of strings going on here?

class HttpRequest {
public:
  enum request_type { GET, POST, PUT, DELETE };

  const request_type GetRequestType() const {
    return request_type_;
  }

  void SetRequestType(const string& request_type) {
    if (request_type == "GET") {
      request_type_ = GET;
    }
  }

  const string& GetPath() const {
    return path_;
  }

  void SetPath(const string& path) {
    path_ = path;
  }

  HttpRequest() {}

private:
  HttpRequest(const HttpRequest&);
  HttpRequest& operator=(const HttpRequest&);

  request_type request_type_;
  string path_;
};

class HttpParser {
public:
  bool Parse(const char data[], int length) {
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

      switch (current_state) {
      case REQUEST_TYPE:
	if (current_char == ' ') {
	  log::Debug("Request [%s]", field.c_str());
	  request.SetRequestType(field);
	  field = "";
	  current_state = PATH;
	} else {
	  field.append(1, current_char);
	}
	break;
      case PATH:
	if (current_char == ' ') {
          log::Debug("Path [%s]", field.c_str());
	  request.SetPath(field);
	  field = "";
	  current_state = PROTOCOL;
	} else {
	  field.append(1, current_char);
	}
	break;
      case PROTOCOL:
	if (current_char == '\n') {
	  log::Debug("Protocol [%s]", field.c_str());
	  field = "";
	  current_state = HEADER_KEY;
	} else {
	  field.append(1, current_char);
	}
	break;
      case HEADER_KEY:
	if (current_char == ':') {
	  log::Debug("Header key [%s]", field.c_str());
	  field = "";
	  current_state = HEADER_DELIM;
	} else {
	  field.append(1, current_char);
	}
	break;
      case HEADER_DELIM:
	log::Debug("Header delim [%d]", current_char);
	if (current_char == ' ') {
	  field = "";
	  current_state = HEADER_VALUE;
	} else {
	  log::Error("Unexpected header delim!");
	}
	break;
      case HEADER_VALUE:
	if (current_char == '\r') {
	  current_char = data[++index]; // assume current_char == '\n' now
	  log::Debug("Header value [%s]", field.c_str());
	  field = "";
	  current_state = HEADER_KEY;
	} else {
	  field.append(1, current_char);
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

  const HttpRequest& GetHttpRequest() {
    return request;
  }

  HttpParser(): index(0), current_state(REQUEST_TYPE), field("") {}

private:
  enum state { REQUEST_TYPE, PATH, PROTOCOL, HEADER_KEY, HEADER_DELIM, HEADER_VALUE, DONE };

  state current_state;
  int index;
  string field;

  HttpRequest request;
};

