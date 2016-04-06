/*
 User Server code for CMPT 276, Spring 2016.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "TableCache.h"
#include "make_unique.h"
#include "ClientUtils.h"


using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_request_options;
using azure::storage::table_result;
using azure::storage::table_shared_access_policy;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using std::make_tuple;
using std::tuple;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34572";
const string auth_addr {"http://localhost:34570/"};
const string addr {"http://localhost:34568/"};

const string sign_on {"SignOn"};
const string sign_off {"SignOff"};
const string add_friend {"AddFriend"};
const string unfriend {"UnFriend"};
const string update_status {"UpdateStatus"};
const string read_friend_list {"ReadFriendList"};

const string data_table_name {"DataTable"};
const string auth_table_name {"AuthTable"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};

const string get_update_data_op {"GetUpdateData"};
const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

//records  users who are signed in
unordered_map<string,tuple<string,string,string>> session;


/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  If the message has no JSON body, return an empty map.

  THIS ROUTINE CAN ONLY BE CALLED ONCE FOR A GIVEN MESSAGE
  (see http://microsoft.github.io/cpprestsdk/classweb_1_1http_1_1http__request.html#ae6c3d7532fe943de75dcc0445456cbc7
  for source of this limit).

  Note that all types of JSON values are returned as strings.
  Use C++ conversion utilities to convert to numbers or dates
  as necessary.
 */
unordered_map<string,string> get_json_body(http_request message) {  
  unordered_map<string,string> results {};
  const http_headers& headers {message.headers()};
  auto content_type (headers.find("Content-Type"));
  if (content_type == headers.end() ||
      content_type->second != "application/json")
    return results;

  value json{};
  message.extract_json(true)
    .then([&json](value v) -> bool
    {
            json = v;
      return true;
    })
    .wait();

  if (json.is_object()) {
    for (const auto& v : json.as_object()) {
      if (v.second.is_string()) {
  results[v.first] = v.second.as_string();
      }
      else {
  results[v.first] = v.second.serialize();
      }
    }
  }
  return results;
}

pair<status_code,value> get_update_data(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_value (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_update_data_op + "/" +
                                              userid,
                                              pwd
                                              )
  };
  //cout << "data: " << result.second << endl;
  if (result.first != status_codes::OK) {
    return make_pair (result.first, value {});
  }
  else {
    return make_pair (result.first, result.second);
  }
}

/*
  Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) { 
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** GET " << path << endl;
  auto paths = uri::split_path(path);

  //No userid
  if (paths.size() < 2) {
    message.reply(status_codes::NotFound);
    return;
  }

  message.reply(status_codes::BadRequest);
  return;
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {

  unordered_map<string,string> json_body {get_json_body(message)};

  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** POST " << path << endl;
  auto paths = uri::split_path(path);

  //No operation and userid, or more parameters than needed
  if (paths.size() != 2) {
    message.reply(status_codes::NotFound);
    return;
  }

  //Sign the person on
  if (paths[0] == sign_on) {
    //Nothing in JSON body
    if (json_body.size() < 1) {
      message.reply(status_codes::NotFound);
      return;
    }

    string pass = json_body[auth_table_password_prop];
    string uid = paths[1];

    cout << "**** SignOn " << uid << " " << pass << endl;

    //cout << "Requesting token and data" << endl;
    pair<status_code, value> token_res {
      get_update_data(auth_addr,
                      uid,
                      pass)
    };
    if (token_res.first != status_codes::OK) {
      message.reply(status_codes::NotFound);
      cout << "SignOn unsuccessful" << endl;
      return;
    }

    string DataRow_val = get_json_object_prop(token_res.second, "DataRow");
    string DataPartition_val = get_json_object_prop(token_res.second, "DataPartition");
    string token_val = get_json_object_prop(token_res.second, "token");

    pair<status_code,value> result {
      do_request (methods::GET,
                  string(addr)
                  + read_entity_auth + "/"
                  + data_table_name + "/"
                  + token_val + "/"
                  + DataPartition_val + "/"
                  + DataRow_val
      )
    };

    if (status_codes::OK == result.first) {

      tuple<string,string,string> data = make_tuple(token_val, DataPartition_val, DataRow_val);
      session.insert({uid, data});

      message.reply(status_codes::OK);
      cout << "SignOn successful" << endl;
      return;
    }
    else{
      message.reply(status_codes::NotFound);
      cout << "SignOn unsuccessful" << endl;
      return;
    }
  }
  //Sign the person off
  else if (paths[0] == sign_off) {
    string uid = paths[1];
    cout << "**** SignOff " << uid << endl;

    for (auto it = session.begin(); it != session.end();) {
      if (it->first == uid) {
        it = session.erase(it);
        message.reply(status_codes::OK);
        cout << "SignOff successful" << endl;
        return;
      }
      else {
        ++it;
      }
    }
    message.reply(status_codes::NotFound);
    cout << "SignOff unsuccessful" << endl;
    return;
  }
  else {
    message.reply(status_codes::NotFound);
    return;
  }

  message.reply(status_codes::BadRequest);
  return;
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PUT " << path << endl;
}


/*
  Main authentication server routine

  Install handlers for the HTTP requests and open the listener,
  which processes each request asynchronously.

  Note that, unlike BasicServer, UserServer only
  installs the listeners for GET. Any other HTTP
  method will produce a Method Not Allowed (405)
  response.

  If you want to support other methods, uncomment
  the call below that hooks in a the appropriate 
  listener.
  
  Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
  cout << "UserServer: Parsing connection string" << endl;

  cout << "UserServer: Opening listener" << endl;
  http_listener listener {def_url};
  listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop UserServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "UserServer closed" << endl;
}
