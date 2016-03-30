/*
 Basic Server code for CMPT 276, Spring 2016.
 */

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cpprest/base_uri.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <was/common.h>
#include <was/storage_account.h>
#include <was/table.h>

#include "ServerUtils.h"
#include "TableCache.h"
#include "make_unique.h"

#include "azure_keys.h"

using azure::storage::cloud_storage_account;
using azure::storage::storage_credentials;
using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_query;
using azure::storage::table_query_iterator;
using azure::storage::table_result;

using pplx::extensibility::critical_section_t;
using pplx::extensibility::scoped_critical_section_t;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_codes;
using web::http::status_code;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_vals_t = vector<pair<string,value>>;

constexpr const char* def_url = "http://localhost:34568";

const string create_table {"CreateTableAdmin"};
const string delete_table {"DeleteTableAdmin"};
const string update_entity {"UpdateEntityAdmin"};
const string delete_entity {"DeleteEntityAdmin"};
// Not implemented, catches and does nothing
const string add_property {"AddPropertyAdmin"};
const string update_property {"UpdatePropertyAdmin"};
const string read_auth {"ReadEntityAuth"};
const string update_auth {"UpdateEntityAuth"};


<<<<<<< HEAD
TableCache table_cache{};
=======
//Cache of open tables is no longer required, so it is deleted
TableCache table_cache {};
>>>>>>> admin_auth

/*
  Convert properties represented in Azure Storage type
  to prop_vals_t type.
 */
prop_vals_t get_properties (const table_entity::properties_type& properties, prop_vals_t values = prop_vals_t {}) {
  for (const auto v : properties) {
    if (v.second.property_type() == edm_type::string) {
      values.push_back(make_pair(v.first, value::string(v.second.string_value())));
    }
    else if (v.second.property_type() == edm_type::datetime) {
      values.push_back(make_pair(v.first, value::string(v.second.str())));
    }
    else if(v.second.property_type() == edm_type::int32) {
      values.push_back(make_pair(v.first, value::number(v.second.int32_value())));      
    }
    else if(v.second.property_type() == edm_type::int64) {
      values.push_back(make_pair(v.first, value::number(v.second.int64_value())));      
    }
    else if(v.second.property_type() == edm_type::double_floating_point) {
      values.push_back(make_pair(v.first, value::number(v.second.double_value())));      
    }
    else if(v.second.property_type() == edm_type::boolean) {
      values.push_back(make_pair(v.first, value::boolean(v.second.boolean_value())));      
    }
    else {
      values.push_back(make_pair(v.first, value::string(v.second.str())));
    }
  }
  return values;
}

/*
  Return true if an HTTP request has a JSON body
 */
bool has_json_body (http_request message) {
  return message.headers()["Content-type"] == "application/json";
}

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  If the message has no JSON body, return an empty map.

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

/*
  Top-level routine for processing all HTTP GET requests.

  GET is the only request that has no command. All
  operands specify the value(s) to be retrieved.
 */
void handle_get(http_request message) {

  unordered_map<string,string> json_body {get_json_body(message)};

  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** GET " << path << endl;
  auto paths = uri::split_path(path);  

  // Need at least a table name
  if (paths.size() < 1) {
    message.reply(status_codes::BadRequest);
    return;
  }

  cloud_table table {table_cache.lookup_table(paths[0])};
  if ( ! table.exists()) {
    message.reply(status_codes::NotFound);
    return;
  }
 

  // GET all entries in table
  if (paths.size() == 1) {
    table_query query {};
    table_query_iterator end;
    table_query_iterator it = table.execute_query(query);
    vector<value> key_vec;

    if (json_body.size() == 0) {
    	cout << "**** No JSON body found" << endl;
	    while (it != end) {
	      cout << "Key: " << it->partition_key() << " / " << it->row_key() << endl;
	      
	      prop_vals_t keys {
	      	make_pair("Partition",value::string(it->partition_key())),
	      	make_pair("Row", value::string(it->row_key()))
	      };
	      
	      keys = get_properties(it->properties(), keys);

		  /*cout << std::get<0>(keys[2]) << endl;      
		  cout << value::object(keys) << endl;
	      cout << it->partition_key() << endl;
	      cout << keys.size() << endl;*/

	      key_vec.push_back(value::object(keys));
	      ++it;
	    }
	    message.reply(status_codes::OK, value::array(key_vec));
	    return;
	}
	else if (json_body.size() > 0) {
		cout << "**** JSON Body found" << endl;

		bool entityExists = false;
		while (it != end) {
			int count = 0;
			bool outputKey = false;
			for(const auto& n : json_body) {
			    
			    prop_vals_t keys {
				    make_pair("Partition",value::string(it->partition_key())),
				    make_pair("Row", value::string(it->row_key()))
				};
			    
			    keys = get_properties(it->properties(), keys);
			    //cout << value::object(keys) << endl;
			    //cout << it->partition_key() << endl;
			    
			    for (int i = 2; i < keys.size(); i = i + 2) {
			    	///cout << "for loop" << endl;
			    	//cout << n.first << "    " << std::get<0>(keys[i]) << endl;
				    if (n.first == std::get<0>(keys[i])) {
				    	//cout << "if statement" << endl;
				    	count++;
				    	if (!outputKey) {
				    		cout << "Key: " << it->partition_key() << " / " << it->row_key() << endl;
				    		outputKey = true;
				    	}
					}
				}
				//cout << count << "          " << keys.size() << endl;
				if(count == keys.size() - 2) {
					key_vec.push_back(value::object(keys));
					entityExists = true;
				}
	    	}
		    ++it;

		}
		if (entityExists) {
	    	message.reply(status_codes::OK, value::array(key_vec));
	    	return;
		}
		else {
			message.reply(status_codes::BadRequest);
		}
	}
	
  }
  else if (paths.size() == 0) {
  	message.reply(status_codes::BadRequest);
  	return;
  }

  // GET specific entry: Partition == paths[1], Row == paths[2]

   // Read entity with authorization, return status code OK,
  // and return entity to be read in JSON array

  pair<status_code,table_entity> authenticator = read_with_token(message, tables_endpoint);

  if (paths[0] == read_auth && std::get<0>(authenticator) == status_codes::OK)
  {
    if (paths.size() < 2) //sample: "http://localhost:34568/TableName/Partition/Row" Tablename = paths[0], Partition = paths[1], Row = paths[2]
    {
	     message.reply(status_codes::BadRequest);
	     return;
    }

    if (paths.size() < 3)
    {
	    message.reply(status_codes::BadRequest);
	    return;
    }
  
    if (paths[2] == "*")
    {
	    table_query query2 {};
	    query2.set_filter_string(azure::storage::table_query::generate_filter_condition(U("PartitionKey"), azure::storage::query_comparison_operator::equal, U(paths[1])));
	    table_query_iterator it = table.execute_query(query2);
	    table_query_iterator end;
	    vector<value> values2_vec {};
  	  bool partition_exists = false;
	    while (it != end) 
      {
	  
	  	  if (it->partition_key() == paths[1]) 
        {
		  	  partition_exists = true;
			    prop_vals_t values2 
			    {
				    make_pair("Partition", value::string(it->partition_key())),
				    make_pair("Row", value::string(it->row_key()))
			    };
			    values2 = get_properties(it->properties(), values2);
			    values2_vec.push_back(value::object(values2));
			    ++it;
		    }
	    }
    

	  if (partition_exists) {
		  message.reply(status_codes::OK, value::array(values2_vec));
		  return;
	  }
	  else {
	  	message.reply(status_codes::BadRequest);
	  	return;
	  }
  }
}
  
  table_operation retrieve_operation {table_operation::retrieve_entity(paths[1],paths[2])};
  table_result retrieve_result {table.execute(retrieve_operation)};
  cout << "HTTP code: " << retrieve_result.http_status_code() << endl;
  if (retrieve_result.http_status_code() == status_codes::NotFound)
  {
    message.reply(status_codes::NotFound);
    return;
  }

  table_entity entity {retrieve_result.entity()};
  table_entity::properties_type properties {entity.properties()};
  
  // If the entity has any properties, return them as JSON
  prop_vals_t values (get_properties(properties));
  vector<pair<string,value>> entityvals 
  {
	  make_pair("Partition", value::string(paths[1])),
	  make_pair("Row", value::string(paths[2]))
  };
  entityvals.insert(entityvals.end(), values.begin(), values.end());
  if (values.size() > 0)
  {
    message.reply(status_codes::OK, value::object(entityvals));
  }
  else
  {
    message.reply(status_codes::OK);
  }
}

/*void get_helper()
{
     // GET specific entry: Partition == paths[1], Row == paths[2]
  if (paths.size() < 2) //sample: "http://localhost:34568/TableName/Partition/Row" Tablename = paths[0], Partition = paths[1], Row = paths[2]
  {
    message.reply(status_codes::BadRequest);
    return;
  }

  if (paths.size() < 3)
  {
    message.reply(status_codes::BadRequest);
    return;
  }
  
    table_query stuff {};
    stuff.set_filter_string(azure::storage::table_query::generate_filter_condition(U("PartitionKey"), azure::storage::query_comparison_operator::equal, U(paths[1])));
    table_query_iterator count = table.execute_query(stuff);
    table_query_iterator end;
    bool partition_exists = false;
    vector<value> auth_vec;

    partition_exists = true;
    prop_vals_t auth_ent 
    {
      make_pair("Partition", value::string(count->partition_key())),
      make_pair("Row", value::string(count->row_key()))
    };

    auth_ent = get_properties(count->properties(), auth_ent);
    auth_vec.push_back(value::object(auth_ent));
    ++count;
    

    if (partition_exists) 
    {
      message.reply(status_codes::OK, value::array(auth_vec));
      return;
    }

    else 
    {
      message.reply(status_codes::BadRequest);
      return;
    }
  
}*/

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** POST " << path << endl;
  auto paths = uri::split_path(path);
  // Need at least an operation and a table name
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }

  string table_name {paths[1]};
  cloud_table table {table_cache.lookup_table(table_name)};

  // Create table (idempotent if table exists)
  if (paths[0] == create_table) {
    cout << "Create " << table_name << endl;
    bool created {table.create_if_not_exists()};
    cout << "Administrative table URI " << table.uri().primary_uri().to_string() << endl;
    if (created)
      message.reply(status_codes::Created);
    else
      message.reply(status_codes::Accepted);
  }
  else {
    message.reply(status_codes::BadRequest);
  }
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) 
{
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PUT " << path << endl;
  auto paths = uri::split_path(path);

  vector<value> update_vec;
  unordered_map<string,string> json_body {get_json_body(message)};

  // Need at least an operation, table name, partition, and row
  if (paths.size() < 4) 
  {
    message.reply(status_codes::BadRequest);
    return;
  }

  cloud_table table {table_cache.lookup_table(paths[1])};
  if ( ! table.exists()) 
  {
    message.reply(status_codes::NotFound);
    return;
  }

  table_entity entity {paths[2], paths[3]};

  // Admin property things that weren't implemented
  if (paths[0] == add_property || paths[0] == update_property)
  {
      message.reply(status_codes::NotImplemented);
  }



  if (update_with_token(message, tables_endpoint, json_body) == status_codes::OK)
  {
    // Update entity
    if (paths[0] == update_entity) 
    {
      cout << "Update " << entity.partition_key() << " / " << entity.row_key() << endl;
      table_entity::properties_type& properties = entity.properties();

      for (const auto v : json_body) 
      {
        properties[v.first] = entity_property {v.second};
      }

      try
      {
          table_operation operation {table_operation::insert_or_merge_entity(entity)};
          table_result op_result {table.execute(operation)};

          message.reply(status_codes::OK);
      }
    

      catch (const storage_exception& e) 
      {
        cout << "Azure Table Storage error: " << e.what() << endl;
        cout << e.result().extended_error().message() << endl;
      
        if (e.result().http_status_code() == status_codes::Forbidden)
        {
          message.reply(status_codes::Forbidden);
        }

        else
        {
          message.reply(status_codes::InternalError);
        }
      }
    }

    else 
    {
      message.reply(status_codes::BadRequest);
    }
  }
}


/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** DELETE " << path << endl;
  auto paths = uri::split_path(path);
  // Need at least an operation and table name
  if (paths.size() < 2) {
	message.reply(status_codes::BadRequest);
	return;
  }

  string table_name {paths[1]};
  cloud_table table {table_cache.lookup_table(table_name)};

  // Delete table
  if (paths[0] == delete_table) {
    cout << "Delete " << table_name << endl;
    if ( ! table.exists()) {
      message.reply(status_codes::NotFound);
    }
    table.delete_table();
    table_cache.delete_entry(table_name);
    message.reply(status_codes::OK);
  }
  // Delete entity
  else if (paths[0] == delete_entity) {
    // For delete entity, also need partition and row
    if (paths.size() < 4) {
	message.reply(status_codes::BadRequest);
	return;
    }
    table_entity entity {paths[2], paths[3]};
    cout << "Delete " << entity.partition_key() << " / " << entity.row_key()<< endl;

    table_operation operation {table_operation::delete_entity(entity)};
    table_result op_result {table.execute(operation)};

    int code {op_result.http_status_code()};
    if (code == status_codes::OK || 
	code == status_codes::NoContent)
      message.reply(status_codes::OK);
    else
      message.reply(code);
  }
  else {
    message.reply(status_codes::BadRequest);
  }
}


/*
  Main server routine

  Install handlers for the HTTP requests and open the listener,
  which processes each request asynchronously.
  
  Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
  http_listener listener {def_url};
  listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  listener.support(methods::PUT, &handle_put);
  listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop server." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "Closed" << endl;
}

