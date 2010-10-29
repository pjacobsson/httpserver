#include <iostream>
#include <string>

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
	cout << "Blank line" << endl;
	break;
      }

      switch (current_state) {
      case REQUEST_TYPE:
	if (current_char == ' ') {
	  cout << "Request [" << field << "]" << endl;
	  request.SetRequestType(field);
	  field = "";
	  current_state = PATH;
	} else {
	  field.append(1, current_char);
	}
	break;
      case PATH:
	if (current_char == ' ') {
	  cout << "Path [" << field << "]" << endl;
	  request.SetPath(field);
	  field = "";
	  current_state = PROTOCOL;
	} else {
	  field.append(1, current_char);
	}
	break;
      case PROTOCOL:
	if (current_char == '\n') {
	  cout << "Protocol [" << field << "]" << endl;
	  field = "";
	  current_state = HEADER_KEY;
	} else {
	  field.append(1, current_char);
	}
	break;
      case HEADER_KEY:
	if (current_char == ':') {
	  cout << "Header key [" << field << "]" << endl;
	  field = "";
	  current_state = HEADER_DELIM;
	} else {
	  field.append(1, current_char);
	}
	break;
      case HEADER_DELIM:
	cout << "Header delim [" << current_char << "]" << endl;
	if (current_char == ' ') {
	  field = "";
	  current_state = HEADER_VALUE;
	} else {
	  cout << "Unexpected header delim!" << endl;
	}
	break;
      case HEADER_VALUE:
	if (current_char == '\r') {
	  current_char = data[++index]; // assume current_char == '\n' now
	  cout << "Header value [" << field << "]" << endl;
	  field = "";
	  current_state = HEADER_KEY;
	} else {
	  field.append(1, current_char);
	}
	break;
      }
      index++;
      if (index == length) {
	cout << "Needs more data" << endl;
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

